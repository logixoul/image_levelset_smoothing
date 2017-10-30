#include "precompiled.h"
#include "stuff.h"
#include "gpgpu.h"
#include "gpuBlur2_3.h"
#include "cfg1.h"
#include "my_console.h"
#include "sw.h"
#include "mainfunc_impl.h"
#include "util.h"
#include "shade.h"
#include "stefanfw.h"

// 13fps

typedef WrapModes::GetWrapped WrapMode;

int wsx=800, wsy=800.0*(800.0/1280.0);
const int scale=4;
Array2D<Vec3f> img;
Array2D<Vec3f> img_in;
bool pause2=false;
std::map<int, gl::Texture> texs;

struct SApp : AppBasic {
	void setup()
	{
		createConsole();
		reset();
		_controlfp(_DN_FLUSH, _MCW_DN);
		
		glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);
		glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
		glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
	}
	void mouseDown(MouseEvent e)
	{
	}
	void keyDown(KeyEvent e)
	{
		keys[e.getChar()] = true;
		if(keys['p'] || keys['2'])
		{
			pause2 = !pause2;
		}
		if(keys['r'])
		{
			reset();
		}
	}
	void reset() {
		img_in = ci::Surface8u(ci::loadImage("test.jpg"));
		int _SX=512/scale;
		img_in = ::resize(img_in, Vec2i(_SX, _SX * float(img_in.h) / float(img_in.w)), ci::FilterTriangle());
		setWindowSize(img_in.w*scale, img_in.h*scale);
		update_();
	}
	void keyUp(KeyEvent e)
	{
		keys[e.getChar()] = false;
	}
	
	typedef Array2D<float> Img;
	void update_2()
	{
		static auto tex_in = gtex(img_in);
		auto tex_state = shade2(tex_in, "_out = vec3(0.0);", ShadeOpts().ifmt(GL_RGBA32F));
		const int lvlstep=5;//15
		for(int lvl=0; lvl<=255; lvl+=lvlstep)
		{
			float flvl = lvl/255.0f;
			globaldict["flvl"] = flvl;
			auto lvlset = shade2(tex_in,
				"vec3 c = fetch3();"
				"c.r = c.r >= flvl ? 1.0 : 0.0;"
				"c.g = c.g >= flvl ? 1.0 : 0.0;"
				"c.b = c.b >= flvl ? 1.0 : 0.0;"
				"_out = c;");
			lvlset.setWrap(GL_REPEAT, GL_REPEAT);
			lvlset = gauss3tex(lvlset);
			lvlset = shade2(lvlset,
				"float fwidth = 1.0 / 3.0; vec3 c = fetch3();"
				"c = smoothstep(.5-fwidth/2.0, .5+fwidth/2.0, c);"
				"_out = c;");
			tex_state = shade2(tex_state, lvlset,
				"vec3 state = fetch3();"
				"vec3 lvlset_val = fetch3(tex2);"
				"_out = mix(state, vec3(flvl), lvlset_val);"
				);
		}

		tex_in = tex_state;
		sw::timeit("draw", [&]() {
			gl::draw(tex_in, getWindowBounds());
		});
	}
	void update_()
	{
		////////////////
		//img=img_in.clone();
		//for(int i = 0; i < 3; i++)
		//	img = blurFaster<Vec3f, WrapModes::Get_WrapZeros>(img, 2);
		//return;
		///////////////
		img = Array2D<Vec3f>(img_in.w, img_in.h, Vec3f::zero());
		for(int c = 0; c < 3; c++)
		{
			const int lvlstep=5;//15
			for(int lvl=0; lvl<=255; lvl+=lvlstep)
			{
				//bool shouldPrint=false;//lvl%40==0;
				//if(shouldPrint)
				//	cout << "==== calculating lvl " << lvl << ", c " << c << ": " << endl;
				float flvl = lvl/255.0f;
				Array2D<float> lvlset(img.w, img.h);
				sw::timeit("create lvlset", [&]() {
					forxy(lvlset) {
						lvlset(p) = img_in(p)[c] >= flvl ? 1.0f : 0.0f;
					}
				});
				sw::timeit("blur", [&]() {
					//lvlset = blurFaster<float, WrapModes::Get_WrapZeros>(lvlset, 1);
					lvlset = gauss3(lvlset);
				});
				sw::timeit("threshold", [&]() {
					//auto grads = get_gradients(lvlset);
					forxy(lvlset) {
						//float fwidth = grads(p).length();
						if(lvlset(p)==0.0f||lvlset(p)==1.0f)
							continue;
						float fwidth = 1.0f / 3.0f;
						lvlset(p)=smoothstep(0.5f-fwidth/2.0f, 0.5f+fwidth/2.0f, lvlset(p));
					}
				});
				sw::timeit("lerp", [&]() {
					forxy(img)
					{
						img(p)[c] = lerp(img(p)[c], flvl, lvlset(p));
					}
				});
			}
		}
		img_in=img;
		sw::timeit("draw", [&]() {
			auto tex=gtex(img);
			gl::draw(tex, getWindowBounds());
		});

	}
	void draw()
	{
		denormal_check::begin_frame();
		mouseX = getMousePos().x / (float)getWindowSize().x;
		mouseY = getMousePos().y / (float)getWindowSize().y;
		
		my_console::beginFrame();
		sw::beginFrame();
		gl::clear(Color(0, 0, 0));
		update_2();
		cfg1::print();
		sw::endFrame();
		my_console::endFrame();

		denormal_check::end_frame();
	}
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	return mainFuncImpl(new SApp());
}

