//
// LICENSE:
//
// Copyright (c) 2016 -- 2017 Fabio Pellacini
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
// this list of conditions and the following disclaimer in the documentation
// and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//

// general includes ------------
#include "yapp.h"

#include <map>
#include <set>

#include "../yocto/yocto_cmd.h"
#include "../yocto/yocto_math.h"
#include "../yocto/yocto_obj.h"
#include "../yocto/yocto_shape.h"

#include "sunsky/ArHosekSkyModel.c"
#include "sunsky/ArHosekSkyModel.h"

template <typename T>
std::vector<T*> concat(const std::vector<T*>& a, const std::vector<T*>& b) {
    std::vector<T*> c;
    for (auto aa : a) c.push_back(aa);
    for (auto bb : b) c.push_back(bb);
    return c;
}

ym::frame3f xform(const ym::vec3f& pos, const ym::vec3f& rot) {
    ym::frame3f xf = ym::identity_frame3f;
    xf = rotation_frame3(ym::vec3f{1, 0, 0}, rot[0] * ym::pif / 180) * xf;
    xf = rotation_frame3(ym::vec3f{0, 1, 0}, rot[1] * ym::pif / 180) * xf;
    xf = rotation_frame3(ym::vec3f{0, 0, 1}, rot[2] * ym::pif / 180) * xf;
    xf = translation_frame3(pos) * xf;
    return xf;
}

ym::frame3f lookat_xform(const ym::vec3f& pos, const ym::vec3f& to) {
    auto xf = lookat_frame3(pos, to, {0, 1, 0});
    xf[2] = -xf[2];
    xf[0] = -xf[0];
    return xf;
}

yapp::shape* make_shape(const std::string& name, yapp::material* mat, int l,
                        yshape::stdsurface_type stype, const ym::vec3f& pos,
                        const ym::vec3f& rot,
                        const ym::vec3f& scale = {1, 1, 1},
                        bool lookat = false) {
    ym::vec4f params = {0.75f, 0.75f, 0, 0};
    auto shape = new yapp::shape();
    shape->name = name;
    shape->mat = mat;
    yshape::make_stdsurface(stype, l, params, shape->triangles, shape->pos,
                            shape->norm, shape->texcoord);
    for (auto& p : shape->pos) (ym::vec3f&)p *= scale;
    if (lookat)
        shape->frame = lookat_xform(pos, rot);
    else
        shape->frame = xform(pos, rot);
    return shape;
}

yapp::shape* make_floor(const std::string& name, yapp::material* mat, float s,
                        float p, int l, const ym::vec3f& pos = {0, 0, 0},
                        const ym::vec3f& rot = {0, 0, 0},
                        const ym::vec3f& scale = {1, 1, 1}) {
    auto n = (int)round(powf(2, (float)l));
    auto shape = new yapp::shape();
    shape->name = name;
    shape->mat = mat;
    yshape::make_uvsurface(n, n, shape->triangles, shape->pos, shape->norm,
                           shape->texcoord,
                           [p, scale](const ym::vec2f& uv) {
                               auto pos = ym::zero3f;
                               auto x = 2 * uv[0] - 1;
                               auto y = 2 * (1 - uv[1]) - 1;
                               if (y >= 0 || !p) {
                                   pos = {x, 0, y};
                               } else {
                                   pos = {x, std::pow(-y, p), y};
                               }
                               return scale * pos;
                           },
                           [](const ym::vec2f& uv) {
                               return ym::vec3f{0, 1, 0};
                           },
                           [s](const ym::vec2f& uv) { return uv * s; });
    if (p) {
        yshape::compute_normals((int)shape->points.size(), shape->points.data(),
                                (int)shape->lines.size(), shape->lines.data(),
                                (int)shape->triangles.size(),
                                shape->triangles.data(), (int)shape->pos.size(),
                                shape->pos.data(), shape->norm.data());
    }
    shape->frame = xform(pos, rot);
    return shape;
}

yapp::material* make_material(const std::string& name, const ym::vec3f& ke,
                              const ym::vec3f& kd, const ym::vec3f& ks,
                              const ym::vec3f& kt, float rs,
                              yapp::texture* ke_txt, yapp::texture* kd_txt,
                              yapp::texture* ks_txt, yapp::texture* kt_txt) {
    auto mat = new yapp::material();
    mat->name = name;
    mat->ke = ke;
    mat->kd = kd;
    mat->ks = ks;
    mat->kt = kt;
    mat->rs = rs;
    mat->ke_txt = ke_txt;
    mat->kd_txt = kd_txt;
    mat->ks_txt = ks_txt;
    mat->kt_txt = kt_txt;
    return mat;
}

yapp::material* make_emission(const std::string& name, const ym::vec3f& ke,
                              yapp::texture* txt = nullptr) {
    return make_material(name, ke, ym::zero3f, ym::zero3f, ym::zero3f, 0, txt,
                         nullptr, nullptr, nullptr);
}

yapp::material* make_diffuse(const std::string& name, const ym::vec3f& kd,
                             yapp::texture* txt = nullptr) {
    return make_material(name, ym::zero3f, kd, ym::zero3f, ym::zero3f, 0,
                         nullptr, txt, nullptr, nullptr);
}

yapp::material* make_plastic(const std::string& name, const ym::vec3f& kd,
                             float rs, yapp::texture* txt = nullptr) {
    return make_material(name, ym::zero3f, kd, {0.04f, 0.04f, 0.04f},
                         ym::zero3f, rs, nullptr, txt, nullptr, nullptr);
}

yapp::material* make_metal(const std::string& name, const ym::vec3f& kd,
                           float rs, yapp::texture* txt = nullptr) {
    return make_material(name, ym::zero3f, ym::zero3f, kd, ym::zero3f, rs,
                         nullptr, nullptr, txt, nullptr);
}

yapp::material* make_glass(const std::string& name, const ym::vec3f& kd,
                           float rs, yapp::texture* txt = nullptr) {
    return make_material(name, ym::zero3f, ym::zero3f, {0.04f, 0.04f, 0.04f},
                         kd, rs, nullptr, nullptr, txt, nullptr);
}

yapp::camera* make_camera(const std::string& name, const ym::vec3f& from,
                          const ym::vec3f& to, float h, float a) {
    auto cam = new yapp::camera();
    cam->name = name;
    cam->frame = lookat_frame3(from, to, {0, 1, 0});
    cam->aperture = a;
    cam->focus = dist(from, to);
    cam->yfov = 2 * atan(h / 2);
    cam->aspect = 16.0f / 9.0f;
    return cam;
}

yapp::environment* make_env(const std::string& name, yapp::material* mat,
                            const ym::vec3f& from, const ym::vec3f& to) {
    auto env = new yapp::environment();
    env->name = name;
    env->mat = mat;
    env->frame = lookat_frame3(from, to, {0, 1, 0});
    return env;
}

yapp::shape* make_points(const std::string& name, yapp::material* mat, int num,
                         const ym::vec3f& pos, const ym::vec3f& rot,
                         const ym::vec3f& scale) {
    auto shape = new yapp::shape();
    shape->name = name;
    shape->mat = mat;

    ym::rng_pcg32 rn;
    yshape::make_points(
        num, shape->points, shape->pos, shape->norm, shape->texcoord,
        shape->radius,
        [&rn, scale](float u) {
            return scale * ym::vec3f{next1f(&rn), next1f(&rn), next1f(&rn)};
        },
        [](float u) {
            return ym::vec3f{0, 0, 1};
        },
        [](float u) {
            return ym::vec2f{u, 0};
        },
        [](float u) { return 0.0025f; });
    shape->frame = xform(pos, rot);
    return shape;
}

yapp::shape* make_lines(const std::string& name, yapp::material* mat, int num,
                        int n, float r, float c, float s, const ym::vec3f& pos,
                        const ym::vec3f& rot, const ym::vec3f& scale) {
    auto shape = new yapp::shape();
    shape->name = name;
    shape->mat = mat;

    ym::rng_pcg32 rn;
    std::vector<ym::vec3f> base(num + 1), dir(num + 1);
    std::vector<float> ln(num + 1);
    for (auto i = 0; i <= num; i++) {
        auto z = -1 + 2 * next1f(&rn);
        auto r = std::sqrt(ym::clamp(1 - z * z, (float)0, (float)1));
        auto phi = 2 * ym::pif * next1f(&rn);
        base[i] = ym::vec3f{r * std::cos(phi), r * std::sin(phi), z};
        dir[i] = base[i];
        ln[i] = 0.15f + 0.15f * next1f(&rn);
    }

    yshape::make_lines(
        n, num, shape->lines, shape->pos, shape->norm, shape->texcoord,
        shape->radius,
        [num, base, dir, ln, r, s, c, &rn, scale](const ym::vec2f& uv) {
            auto i = ym::clamp((int)(uv[1] * (num + 1)), 0, num);
            auto pos = base[i] * (1 + uv[0] * ln[i]);
            if (r) {
                pos += ym::vec3f{r * (0.5f - next1f(&rn)),
                                 r * (0.5f - next1f(&rn)),
                                 r * (0.5f - next1f(&rn))};
            }
            if (s && uv[0]) {
                ym::frame3f rotation =
                    rotation_frame3(ym::vec3f{0, 1, 0}, s * uv[0] * uv[0]);
                pos = transform_point(rotation, pos);
            }
            auto nc = 128;
            if (c && i > nc) {
                int cc = 0;
                float md = HUGE_VALF;
                for (int k = 0; k < nc; k++) {
                    float d = dist(base[i], base[k]);
                    if (d < md) {
                        md = d;
                        cc = k;
                    }
                }
                ym::vec3f cpos = base[cc] * (1 + uv[0] * ln[cc]);
                pos =
                    pos * (1 - c * uv[0] * uv[0]) + cpos * (c * uv[0] * uv[0]);
            }
            return scale * pos;
        },
        [](const ym::vec2f& uv) {
            return ym::vec3f{0, 0, 1};
        },
        [](const ym::vec2f& uv) { return uv; },
        [](const ym::vec2f& uv) { return 0.001f + 0.001f * (1 - uv[0]); });

    yshape::compute_normals((int)shape->points.size(), shape->points.data(),
                            (int)shape->lines.size(), shape->lines.data(),
                            (int)shape->triangles.size(),
                            shape->triangles.data(), (int)shape->pos.size(),
                            shape->pos.data(), shape->norm.data());
    shape->frame = xform(pos, rot);
    return shape;
}

std::vector<yapp::texture*> make_random_textures() {
    const std::string txts[5] = {"grid.png", "checker.png", "rchecker.png",
                                 "colored.png", "rcolored.png"};
    std::vector<yapp::texture*> textures;
    for (auto txt : txts) {
        textures.push_back(new yapp::texture());
        textures.back()->path = txt;
    }
    return textures;
}

std::vector<yapp::material*> make_random_materials(int nshapes) {
    auto textures = make_random_textures();
    std::vector<yapp::material*> materials(nshapes);
    materials[0] = make_diffuse("floor", {1, 1, 1}, textures[0]);

    ym::rng_pcg32 rn;
    for (auto i = 1; i < nshapes; i++) {
        char name[1024];
        sprintf(name, "obj%02d", i);
        auto txt = -1;
        if (next1f(&rn) < 0.5f) {
            txt = (int)(next1f(&rn) * 6) - 1;
        }
        auto c = (txt >= 0) ? ym::vec3f{1, 1, 1}
                            : ym::vec3f{0.2f + 0.3f * next1f(&rn),
                                        0.2f + 0.3f * next1f(&rn),
                                        0.2f + 0.3f * next1f(&rn)};
        auto rs = 0.01f + 0.25f * next1f(&rn);
        auto mt = (int)(next1f(&rn) * 4);
        if (mt == 0) {
            materials[i] =
                make_diffuse(name, c, (txt < 0) ? nullptr : textures[txt]);
        } else if (mt == 1) {
            materials[i] =
                make_metal(name, c, rs, (txt < 0) ? nullptr : textures[txt]);
        } else {
            materials[i] =
                make_plastic(name, c, rs, (txt < 0) ? nullptr : textures[txt]);
        }
    }

    return materials;
}

std::vector<yapp::shape*> make_random_shapes(int nshapes, int l) {
    auto materials = make_random_materials(nshapes);
    std::vector<yapp::shape*> shapes(nshapes);
    shapes[0] = make_floor("floor", materials[0], 6, 4, 6, {0, 0, -4},
                           ym::zero3f, {6, 6, 6});

    ym::vec3f pos[1024];
    float radius[1024];
    int levels[1024];

    ym::rng_pcg32 rn;
    for (auto i = 1; i < nshapes; i++) {
        auto done = false;
        while (!done) {
            auto x = -2 + 4 * next1f(&rn);
            auto z = 1 - 3 * next1f(&rn);
            radius[i] = 0.15f + ((1 - z) / 3) * ((1 - z) / 3) * 0.5f;
            pos[i] = ym::vec3f{x, radius[i], z};
            levels[i] = (int)round(log2f(powf(2, (float)l) * radius[i] / 0.5f));
            done = true;
            for (int j = 1; j < i && done; j++) {
                if (dist(pos[i], pos[j]) < radius[i] + radius[j]) done = false;
            }
        }
    }

    for (auto i = 1; i < nshapes; i++) {
        char name[1024];
        sprintf(name, "obj%02d", i);
        yshape::stdsurface_type stypes[3] = {
            yshape::stdsurface_type::uvspherecube,
            yshape::stdsurface_type::uvspherizedcube,
            yshape::stdsurface_type::uvflipcapsphere,
        };
        auto stype = stypes[(int)(next1f(&rn) * 3)];
        if (stype == yshape::stdsurface_type::uvflipcapsphere) levels[i]++;
        shapes[i] = make_shape(name, materials[0], levels[i], stype, pos[i],
                               ym::zero3f, {radius[i], radius[i], radius[i]});
    }

    return shapes;
}

std::vector<yapp::shape*> make_random_rigid_shapes(
    int nshapes, int l, const std::vector<yapp::material*>& materials) {
    std::vector<yapp::shape*> shapes(nshapes);
    shapes[0] =
        make_shape("floor", materials[0], 2, yshape::stdsurface_type::uvcube,
                   {0, -0.5, 0}, ym::zero3f, {6, 0.5, 6});
    ym::vec3f pos[1024];
    float radius[1024];
    int levels[1024];

    ym::rng_pcg32 rn;
    for (int i = 1; i < nshapes; i++) {
        bool done = false;
        while (!done) {
            radius[i] = 0.1f + 0.4f * next1f(&rn);
            pos[i] = ym::vec3f{-2 + 4 * next1f(&rn), 1 + 4 * next1f(&rn),
                               -2 + 4 * next1f(&rn)};
            levels[i] = (int)round(log2f(powf(2, (float)l) * radius[i] / 0.5f));
            done = true;
            for (int j = 1; j < i && done; j++) {
                if (dist(pos[i], pos[j]) < radius[i] + radius[j]) done = false;
            }
        }
    }

    for (int i = 1; i < nshapes; i++) {
        auto name = "obj" + std::to_string(i);
        yshape::stdsurface_type stypes[2] = {
            yshape::stdsurface_type::uvspherecube,
            yshape::stdsurface_type::uvcube};
        auto stype = stypes[(int)(next1f(&rn) * 2)];
        shapes[i] = make_shape(name, materials[i], levels[i], stype, pos[i],
                               ym::zero3f, {radius[i], radius[i], radius[i]});
    }

    return shapes;
}

yapp::scene* make_scene(const std::vector<yapp::camera*>& cameras,
                        const std::vector<yapp::shape*>& shapes,
                        const std::vector<yapp::environment*>& envs = {}) {
    auto scene = new yapp::scene();
    scene->cameras = cameras;
    auto materials = std::set<yapp::material*>();
    auto textures = std::set<yapp::texture*>();
    for (auto shp : shapes) {
        scene->shapes.push_back(shp);
        assert(shp->mat);
        materials.insert(shp->mat);
    }
    for (auto env : envs) {
        scene->environments.push_back(env);
        assert(env->mat);
        materials.insert(env->mat);
    }
    for (auto mat : materials) {
        scene->materials.push_back(mat);
        textures.insert(mat->ke_txt);
        textures.insert(mat->kd_txt);
        textures.insert(mat->ks_txt);
        textures.insert(mat->kt_txt);
        textures.insert(mat->rs_txt);
    }
    textures.erase(nullptr);
    for (auto txt : textures) {
        scene->textures.push_back(txt);
    }
    return scene;
}

using ubyte = unsigned char;
struct rgba {
    ubyte r, g, b, a;
};

std::vector<rgba> make_grid(int s) {
    std::vector<rgba> pixels(s * s);
    int g = 64;
    for (int j = 0; j < s; j++) {
        for (int i = 0; i < s; i++) {
            if (i % g == 0 || i % g == g - 1 || j % g == 0 || j % g == g - 1)
                pixels[j * s + i] = rgba{90, 90, 90, 255};
            else
                pixels[j * s + i] = rgba{128, 128, 128, 255};
        }
    }
    return pixels;
}

std::vector<rgba> make_checker(int s) {
    std::vector<rgba> pixels(s * s);
    for (int j = 0; j < s; j++) {
        for (int i = 0; i < s; i++) {
            if ((i / 64 + j / 64) % 2)
                pixels[j * s + i] = rgba{90, 90, 90, 255};
            else
                pixels[j * s + i] = rgba{128, 128, 128, 255};
        }
    }
    return pixels;
}

// http://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
rgba hsv_to_rgb(ubyte h, ubyte s, ubyte v) {
    rgba rgb = {0, 0, 0, 255};
    ubyte region, remainder, p, q, t;

    if (s == 0) {
        rgb.r = v;
        rgb.g = v;
        rgb.b = v;
        return rgb;
    }

    region = h / 43;
    remainder = (h - (region * 43)) * 6;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0:
            rgb.r = v;
            rgb.g = t;
            rgb.b = p;
            break;
        case 1:
            rgb.r = q;
            rgb.g = v;
            rgb.b = p;
            break;
        case 2:
            rgb.r = p;
            rgb.g = v;
            rgb.b = t;
            break;
        case 3:
            rgb.r = p;
            rgb.g = q;
            rgb.b = v;
            break;
        case 4:
            rgb.r = t;
            rgb.g = p;
            rgb.b = v;
            break;
        default:
            rgb.r = v;
            rgb.g = p;
            rgb.b = q;
            break;
    }

    return rgb;
}

std::vector<rgba> make_rcolored(int s) {
    std::vector<rgba> pixels(s * s);
    for (int j = 0; j < s; j++) {
        for (int i = 0; i < s; i++) {
            ubyte ph = 32 * (i / (s / 8));
            ubyte pv = 128;
            ubyte ps = 64 + 16 * (7 - j / (s / 8));
            if (i % 32 && j % 32) {
                if ((i / 64 + j / 64) % 2)
                    pv += 16;
                else
                    pv -= 16;
                if ((i / 16 + j / 16) % 2)
                    pv += 4;
                else
                    pv -= 4;
                if ((i / 4 + j / 4) % 2)
                    pv += 1;
                else
                    pv -= 1;
            } else {
                pv = 196;
                ps = 32;
            }
            pixels[j * s + i] = hsv_to_rgb(ph, ps, pv);
        }
    }
    return pixels;
}

std::vector<rgba> make_gammaramp(int s) {
    std::vector<rgba> pixels(s * s);
    for (int j = 0; j < s; j++) {
        for (int i = 0; i < s; i++) {
            auto u = j / float(s - 1);
            if (i < s / 3) u = pow(u, 2.2f);
            if (i > (s * 2) / 3) u = pow(u, 1 / 2.2f);
            auto c = (unsigned char)(u * 255);
            pixels[j * s + i] = {c, c, c, 255};
        }
    }
    return pixels;
}

std::vector<ym::vec4f> make_gammarampf(int s) {
    std::vector<ym::vec4f> pixels(s * s);
    for (int j = 0; j < s; j++) {
        for (int i = 0; i < s; i++) {
            auto u = j / float(s - 1);
            if (i < s / 3) u = pow(u, 2.2f);
            if (i > (s * 2) / 3) u = pow(u, 1 / 2.2f);
            pixels[j * s + i] = {u, u, u, 1};
        }
    }
    return pixels;
}

std::vector<rgba> make_colored(int s) {
    std::vector<rgba> pixels(s * s);
    for (int j = 0; j < s; j++) {
        for (int i = 0; i < s; i++) {
            ubyte ph = 32 * (i / (s / 8));
            ubyte pv = 128;
            ubyte ps = 64 + 16 * (7 - j / (s / 8));
            if (i % 32 && j % 32) {
                if ((i / 64 + j / 64) % 2)
                    pv += 16;
                else
                    pv -= 16;
            } else {
                pv = 196;
                ps = 32;
            }
            pixels[j * s + i] = hsv_to_rgb(ph, ps, pv);
        }
    }
    return pixels;
}

std::vector<rgba> make_rchecker(int s) {
    std::vector<rgba> pixels(s * s);
    for (int j = 0; j < s; j++) {
        for (int i = 0; i < s; i++) {
            ubyte pv = 128;
            if (i % 32 && j % 32) {
                if ((i / 64 + j / 64) % 2)
                    pv += 16;
                else
                    pv -= 16;
                if ((i / 16 + j / 16) % 2)
                    pv += 4;
                else
                    pv -= 4;
                if ((i / 4 + j / 4) % 2)
                    pv += 1;
                else
                    pv -= 1;
            } else {
                pv = 196;
            }
            pixels[j * s + i] = rgba{pv, pv, pv, 255};
        }
    }
    return pixels;
}

#define sqr(x) ((x) * (x))

std::vector<ym::vec4f> make_sunsky_hdr(int w, int h, float sun_theta,
                                       float turbidity, ym::vec3f ground,
                                       float scale, bool include_ground) {
    std::vector<ym::vec4f> rgba(w * h);
    ArHosekSkyModelState* skymodel_state[3] = {
        arhosek_rgb_skymodelstate_alloc_init(turbidity, ground[0], sun_theta),
        arhosek_rgb_skymodelstate_alloc_init(turbidity, ground[0], sun_theta),
        arhosek_rgb_skymodelstate_alloc_init(turbidity, ground[0], sun_theta),
    };
    auto sun_phi = ym::pif;
    auto sun_w = ym::vec3f{cosf(sun_phi) * sinf(sun_theta),
                           sinf(sun_phi) * sinf(sun_theta), cosf(sun_theta)};
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            auto theta = ym::pif * (j + 0.5f) / h;
            auto phi = 2 * ym::pif * (i + 0.5f) / w;
            if (include_ground)
                theta = ym::clamp(theta, 0.0f, ym::pif / 2 - 0.001f);
            auto pw =
                ym::vec3f{std::cos(phi) * std::sin(theta),
                          std::sin(phi) * std::sin(theta), std::cos(theta)};
            auto gamma =
                std::acos(ym::clamp(dot(sun_w, pw), (float)-1, (float)1));
            auto sky = ym::vec3f{(float)(arhosek_tristim_skymodel_radiance(
                                     skymodel_state[0], theta, gamma, 0)),
                                 (float)(arhosek_tristim_skymodel_radiance(
                                     skymodel_state[1], theta, gamma, 1)),
                                 (float)(arhosek_tristim_skymodel_radiance(
                                     skymodel_state[2], theta, gamma, 2))};
            rgba[j * w + i] = {scale * sky[0], scale * sky[1], scale * sky[2],
                               1};
        }
    }
    arhosekskymodelstate_free(skymodel_state[0]);
    arhosekskymodelstate_free(skymodel_state[1]);
    arhosekskymodelstate_free(skymodel_state[2]);
    return rgba;
}

void save_image(const std::string& filename, const std::string& dirname,
                const rgba* pixels, int s) {
    std::string path = std::string(dirname) + "/" + std::string(filename);
    stbi_write_png(path.c_str(), s, s, 4, pixels, s * 4);
}

void save_image_hdr(const std::string& filename, const std::string& dirname,
                    const ym::vec4f* pixels, int w, int h) {
    std::string path = std::string(dirname) + "/" + std::string(filename);
    stbi_write_hdr(path.c_str(), w, h, 4, (float*)pixels);
}

void save_scene(const std::string& filename, const std::string& dirname,
                const yapp::scene* scn) {
    yapp::save_scene(dirname + "/" + filename, scn);
    yapp::save_scene(dirname + "/" + ycmd::get_basename(filename) + ".gltf",
                     scn);
}

yapp::texture* make_texture(const std::string& path) {
    auto txt = new yapp::texture();
    txt->path = path;
    return txt;
}

yapp::shape* make_point(const std::string& name, yapp::material* mat,
                        const ym::vec3f& pos = {0, 0, 0},
                        float radius = 0.001f) {
    auto shape = new yapp::shape();
    shape->name = name;
    shape->mat = mat;
    shape->points.push_back(0);
    shape->pos.push_back(pos);
    shape->norm.push_back({0, 0, 1});
    shape->radius.push_back(radius);
    return shape;
}

std::vector<yapp::camera*> make_simple_cameras() {
    return {make_camera("cam", {0, 1.5f, 5}, {0, 0.5f, 0}, 0.5f, 0),
            make_camera("cam_dof", {0, 1.5f, 5}, {0, 0.5, 0}, 0.5f, 0.1f)};
}

std::vector<yapp::material*> make_simple_lightmaterials(bool arealights) {
    if (!arealights) {
        return {
            make_emission("light01", {100, 100, 100}),
            make_emission("light02", {100, 100, 100}),
        };
    } else {
        return {
            make_emission("light01", {40, 40, 40}),
            make_emission("light02", {40, 40, 40}),
        };
    }
}

std::vector<yapp::shape*> make_simple_lights(bool arealights) {
    auto materials = make_simple_lightmaterials(arealights);
    if (!arealights) {
        return {make_point("light01", materials[0], {0.7f, 4, 3}),
                make_point("light02", materials[1], {-0.7f, 4, 3})};
    } else {
        return {make_shape("light01", materials[0], 0,
                           yshape::stdsurface_type::uvquad, {2, 2, 4},
                           {0, 1, 0}, {1, 1, 1}, true),
                make_shape("light02", materials[1], 0,
                           yshape::stdsurface_type::uvquad, {-2, 2, 4},
                           {0, 1, 0}, {1, 1, 1}, true)};
    }
}

yapp::scene* make_simple_scene(bool textured, bool arealights) {
    std::vector<yapp::material*> materials;
    if (!textured) {
        materials = std::vector<yapp::material*>{
            make_diffuse("floor", {0.2f, 0.2f, 0.2f}),
            make_plastic("obj01", {0.5f, 0.2f, 0.2f}, 0.1f),
            make_plastic("obj02", {0.2f, 0.5f, 0.2f}, 0.05f),
            make_plastic("obj03", {0.2f, 0.2f, 0.5f}, 0.01f)};
    } else {
        auto textures = std::vector<yapp::texture*>{
            make_texture("grid.png"), make_texture("rcolored.png"),
            make_texture("checker.png"), make_texture("colored.png"),
        };
        materials = std::vector<yapp::material*>{
            make_diffuse("floor", {1, 1, 1}, textures[0]),
            make_plastic("obj01", {1, 1, 1}, 0.1f, textures[1]),
            make_plastic("obj02", {1, 1, 1}, 0.05f, textures[2]),
            make_plastic("obj03", {1, 1, 1}, 0.01f, textures[3])};
    }
    std::vector<yapp::shape*> shapes = {
        make_floor("floor", materials[0], 6, 4, 6, {0, 0, -4}, ym::zero3f,
                   {6, 6, 6}),
        make_shape("obj01", materials[1], 5,
                   yshape::stdsurface_type::uvflipcapsphere, {-1.25f, 0.5f, 0},
                   ym::zero3f, {0.5f, 0.5f, 0.5f}),
        make_shape("obj02", materials[2], 4,
                   yshape::stdsurface_type::uvspherizedcube, {0, 0.5f, 0},
                   ym::zero3f, {0.5f, 0.5f, 0.5f}),
        make_shape("obj03", materials[3], 4,
                   yshape::stdsurface_type::uvspherecube, {1.25f, 0.5f, 0},
                   ym::zero3f, {0.5f, 0.5f, 0.5f})};
    return make_scene(make_simple_cameras(),
                      concat(shapes, make_simple_lights(arealights)));
}

yapp::scene* make_pointslines_scene(bool lines, bool arealights) {
    std::vector<yapp::shape*> shapes;
    std::vector<yapp::material*> materials;
    std::vector<yapp::texture*> textures;
    materials =
        std::vector<yapp::material*>{make_diffuse("floor", {0.2f, 0.2f, 0.2f}),
                                     make_diffuse("obj", {0.2f, 0.2f, 0.2f}),
                                     make_diffuse("points", {0.2f, 0.2f, 0.2f}),
                                     make_diffuse("lines", {0.2f, 0.2f, 0.2f})};
    shapes.push_back(make_floor("floor", materials[0], 6, 4, 6, {0, 0, -4},
                                ym::zero3f, {6, 6, 6}));
    if (!lines) {
        shapes.push_back(make_points("points01", materials[2], 64 * 64 * 16,
                                     {0, 0.5f, 0}, ym::zero3f,
                                     {0.5f, 0.5f, 0.5f}));
    } else {
        shapes.push_back(make_shape(
            "obj01", materials[1], 6, yshape::stdsurface_type::uvsphere,
            {1.25f, 0.5f, 0}, ym::zero3f, {0.5f, 0.5f, 0.5f}));
        shapes.push_back(make_lines("lines01", materials[3], 64 * 64 * 16, 4,
                                    0.1f, 0, 0, {1.25f, 0.5f, 0}, ym::zero3f,
                                    {0.5f, 0.5f, 0.5f}));
        shapes.push_back(make_shape(
            "obj02", materials[1], 6, yshape::stdsurface_type::uvsphere,
            {0, 0.5f, 0}, ym::zero3f, {0.5f, 0.5f, 0.5f}));
        shapes.push_back(make_lines("lines02", materials[3], 64 * 64 * 16, 4, 0,
                                    0.75f, 0, {0, 0.5f, 0}, ym::zero3f,
                                    {0.5f, 0.5f, 0.5f}));
        shapes.push_back(make_shape(
            "obj03", materials[1], 6, yshape::stdsurface_type::uvsphere,
            {-1.25f, 0.5f, 0}, ym::zero3f, {0.5f, 0.5f, 0.5f}));
        shapes.push_back(make_lines("lines03", materials[3], 64 * 64 * 16, 4, 0,
                                    0, 0.5f, {-1.25f, 0.5f, 0}, ym::zero3f,
                                    {0.5f, 0.5f, 0.5f}));
    }

    return make_scene(make_simple_cameras(),
                      concat(shapes, make_simple_lights(arealights)));
}

yapp::scene* make_random_scene(int nshapes, bool arealights) {
    std::vector<yapp::camera*> cameras = {
        make_camera("cam", {0, 1.5f, 5}, {0, 0.5f, 0}, 0.5f, 0),
        make_camera("cam_dof", {0, 1.5f, 5}, {0, 0.5, 0}, 0.5f, 0.1f)};
    std::vector<yapp::shape*> shapes = make_random_shapes(nshapes, 5);
    return make_scene(cameras, concat(shapes, make_simple_lights(arealights)));
}

// http://graphics.cs.williams.edu/data
// http://www.graphics.cornell.edu/online/box/data.html
yapp::scene* make_cornell_box_scene() {
    std::vector<yapp::camera*> cameras = {
        make_camera("cam", {0, 1, 4}, {0, 1, 0}, 0.7f, 0)};
    std::vector<yapp::material*> materials = {
        make_diffuse("white", {0.725f, 0.71f, 0.68f}),
        make_diffuse("red", {0.63f, 0.065f, 0.05f}),
        make_diffuse("green", {0.14f, 0.45f, 0.091f}),
        make_emission("light", {17, 12, 4}),
    };
    std::vector<yapp::shape*> shapes = {
        make_shape("floor", materials[0], 0, yshape::stdsurface_type::uvquad,
                   ym::zero3f, {-90, 0, 0}),
        make_shape("ceiling", materials[0], 0, yshape::stdsurface_type::uvquad,
                   {0, 2, 0}, {90, 0, 0}),
        make_shape("back", materials[0], 0, yshape::stdsurface_type::uvquad,
                   {0, 1, -1}, ym::zero3f),
        make_shape("back", materials[2], 0, yshape::stdsurface_type::uvquad,
                   {+1, 1, 0}, {0, -90, 0}),
        make_shape("back", materials[1], 0, yshape::stdsurface_type::uvquad,
                   {-1, 1, 0}, {0, 90, 0}),
        make_shape("tallbox", materials[0], 0, yshape::stdsurface_type::uvcube,
                   {-0.33f, 0.6f, -0.29f}, {0, 15, 0}, {0.3f, 0.6f, 0.3f}),
        make_shape("shortbox", materials[0], 0, yshape::stdsurface_type::uvcube,
                   {0.33f, 0.3f, 0.33f}, {0, -15, 0}, {0.3f, 0.3f, 0.3f}),
        make_shape("light", materials[3], 0, yshape::stdsurface_type::uvquad,
                   {0, 1.999f, 0}, {90, 0, 0}, {0.25f, 0.25f, 0.25f})};
    return make_scene(cameras, shapes, {});
}

yapp::scene* make_envmap_scene(bool as_shape, bool use_map) {
    std::vector<yapp::camera*> cameras = {
        make_camera("cam", {0, 1.5f, 5}, {0, 0.5f, 0}, 0.5f, 0),
        make_camera("cam_dof", {0, 1.5f, 5}, {0, 0.5f, 0}, 0.5f, 0.1f)};
    std::vector<yapp::material*> materials = {
        make_diffuse("floor", {0.2f, 0.2f, 0.2f}),
        make_plastic("obj01", {0.5f, 0.2f, 0.2f}, 0.1f),
        make_plastic("obj02", {0.2f, 0.5f, 0.2f}, 0.05f),
        make_plastic("obj03", {0.2f, 0.2f, 0.5f}, 0.01f),
        make_emission("env", {1, 1, 1},
                      (use_map) ? make_texture("env.hdr") : nullptr)};
    std::vector<yapp::shape*> shapes = {
        make_floor("floor", materials[0], 6, 4, 6, {0, 0, -4}, ym::zero3f,
                   {6, 6, 6}),
        make_shape("obj01", materials[1], 5,
                   yshape::stdsurface_type::uvflipcapsphere, {-1.25f, 0.5f, 0},
                   ym::zero3f, {0.5f, 0.5f, 0.5f}),
        make_shape("obj02", materials[2], 4,
                   yshape::stdsurface_type::uvspherizedcube, {0, 0.5f, 0},
                   ym::zero3f, {0.5f, 0.5f, 0.5f}),
        make_shape("obj03", materials[3], 4,
                   yshape::stdsurface_type::uvspherecube, {1.25f, 0.5f, 0},
                   ym::zero3f, {0.5f, 0.5f, 0.5f})};
    std::vector<yapp::environment*> environments;
    if (as_shape) {
        shapes.push_back(make_shape("env_sphere", materials[4], 6,
                                    yshape::stdsurface_type::uvflippedsphere,
                                    {0, 0.5f, 0}, {-90, 0, 0},
                                    {10000, 10000, 10000}));
    } else {
        environments.push_back(
            make_env("env", materials[4], {0, 0.5f, 0}, {-1.5f, 0.5f, 0}));
    }

    return make_scene(cameras, shapes, environments);
}

yapp::scene* make_mat_scene(int mat, bool use_map) {
    std::vector<yapp::camera*> cameras = {
        make_camera("cam", {0, 1.5f, 5}, {0, 0.5f, 0}, 0.5f, 0),
        make_camera("cam_dof", {0, 1.5f, 5}, {0, 0.5f, 0}, 0.5f, 0.1f)};
    std::vector<yapp::material*> materials;
    std::vector<yapp::texture*> textures = {make_texture("grid.png")};
    switch (mat) {
        case 0: {
            materials = {make_diffuse("floor", {0.1f, 0.1f, 0.1f}, textures[0]),
                         make_diffuse("int", {0.2f, 0.2f, 0.2f}),
                         make_plastic("obj01", {0.5f, 0.2f, 0.2f}, 0.1f),
                         make_plastic("obj02", {0.2f, 0.5f, 0.2f}, 0.05f),
                         make_plastic("obj03", {0.2f, 0.2f, 0.5f}, 0.01f)};
        } break;
        case 1: {
            materials = {make_diffuse("floor", {0.1f, 0.1f, 0.1f}, textures[0]),
                         make_diffuse("int", {0.2f, 0.2f, 0.2f}),
                         make_metal("obj01", {0.9f, 0.9f, 0.9f}, 0.0f),
                         make_metal("obj02", {0.9f, 0.9f, 0.9f}, 0.05f),
                         make_plastic("obj03", {0.2f, 0.2f, 0.2f}, 0.01f)};
        } break;
        case 2: {
            materials = {make_diffuse("floor", {0.1f, 0.1f, 0.1f}, textures[0]),
                         make_diffuse("int", {0.2f, 0.2f, 0.2f}),
                         make_glass("obj01", {0.8f, 0.8f, 0.8f}, 0.0f),
                         make_plastic("obj02", {0.2f, 0.5f, 0.2f}, 0.05f),
                         make_plastic("obj03", {0.2f, 0.2f, 0.5f}, 0.01f)};
        } break;
        default: assert(false);
    }
    std::vector<yapp::shape*> shapes = {
        make_floor("floor", materials[0], 6, 4, 6, {0, 0, -4}, ym::zero3f,
                   {6, 6, 6}),
        make_shape("int01", materials[1], 5, yshape::stdsurface_type::uvsphere,
                   {-1.25f, 0.5f, 0}, ym::zero3f, {0.4f, 0.4f, 0.4f}),
        make_shape("int02", materials[1], 5, yshape::stdsurface_type::uvsphere,
                   {0, 0.5f, 0}, ym::zero3f, {0.4f, 0.4f, 0.4f}),
        make_shape("int03", materials[1], 5, yshape::stdsurface_type::uvsphere,
                   {1.25f, 0.5f, 0}, ym::zero3f, {0.4f, 0.4f, 0.4f}),
        make_shape("obj01", materials[2], 5,
                   yshape::stdsurface_type::uvflipcapsphere, {-1.25f, 0.5f, 0},
                   {0, 35, 45}, {0.5f, 0.5f, 0.5f}),
        make_shape("obj02", materials[3], 4,
                   yshape::stdsurface_type::uvflipcapsphere, {0, 0.5f, 0},
                   {0, 35, 45}, {0.5f, 0.5f, 0.5f}),
        make_shape("obj03", materials[4], 4,
                   yshape::stdsurface_type::uvflipcapsphere, {1.25f, 0.5f, 0},
                   {0, 35, 45}, {0.5f, 0.5f, 0.5f})};
    if (use_map) {
        std::vector<yapp::environment*> environments;
        environments.push_back(make_env(
            "env", make_emission("env", {1, 1, 1}, make_texture("env.hdr")),
            {0, 0.5f, 0}, {-1.5f, 0.5f, 0}));
        return make_scene(cameras, shapes, environments);
    } else {
        return make_scene(cameras, concat(shapes, make_simple_lights(true)));
    }
}

yapp::scene* make_trans_scene(int mat, bool use_map) {
    std::vector<yapp::camera*> cameras = {
        make_camera("cam", {0, 1.5f, 5}, {0, 0.5f, 0}, 0.5f, 0),
        make_camera("cam_dof", {0, 1.5f, 5}, {0, 0.5f, 0}, 0.5f, 0.1f)};
    std::vector<yapp::material*> materials;
    std::vector<yapp::texture*> textures;
    textures.push_back(make_texture("grid.png"));
    switch (mat) {
        case 0: {
            materials = {make_diffuse("floor", {0.1f, 0.1f, 0.1f}, 0),
                         make_diffuse("int", {0.2f, 0.2f, 0.2f}),
                         make_glass("obj01", {0.8f, 0.8f, 0.8f}, 0.0f),
                         make_glass("obj02", {0.8f, 0.8f, 0.8f}, 0.0f),
                         make_glass("obj03", {0.8f, 0.2f, 0.2f}, 0.0f)};
        } break;
        //        case 1: {
        //            materials = {make_diffuse("floor", {0.1f, 0.1f, 0.1f}, 0),
        //                make_diffuse("int", {0.2f, 0.2f, 0.2f}),
        //                make_metal("obj01", {0.9f, 0.9f, 0.9f}, 0.0f),
        //                make_metal("obj02", {0.9f, 0.9f, 0.9f}, 0.05f),
        //                make_plastic("obj03", {0.2f, 0.2f, 0.2f}, 0.01f)};
        //        } break;
        //        case 2: {
        //            materials = {make_diffuse("floor", {0.1f, 0.1f, 0.1f}, 0),
        //                make_diffuse("int", {0.2f, 0.2f, 0.2f}),
        //                make_glass("obj01", 0.0f),
        //                make_plastic("obj02", {0.2f, 0.5f, 0.2f}, 0.05f),
        //                make_plastic("obj03", {0.2f, 0.2f, 0.5f}, 0.01f)};
        //        } break;
        default: assert(false);
    }
    std::vector<yapp::shape*> shapes = {
        make_floor("floor", materials[0], 6, 4, 6, {0, 0, -4}, ym::zero3f,
                   {6, 6, 6}),
        //        make_shape("int01", 1, 5, yshape::stdsurface_type::uvsphere,
        //                    {-1.25f, 0.5f, 0}, ym::zero3f, {0.4f, 0.4f,
        //                    0.4f}),
        //        make_shape("int02", 1, 5, yshape::stdsurface_type::uvsphere,
        //                   {0, 0.5f, 0}, ym::zero3f, {0.4f, 0.4f, 0.4f}),
        //        make_shape("int03", 1, 5, yshape::stdsurface_type::uvsphere,
        //                   {1.25f, 0.5f, 0}, ym::zero3f, {0.4f, 0.4f, 0.4f}),
        make_shape("obj01", materials[2], 5,
                   yshape::stdsurface_type::uvflipcapsphere, {-1.25f, 0.5f, 0},
                   {0, 35, 45}, {0.5f, 0.5f, 0.5f}),
        make_shape("obj02", materials[3], 4, yshape::stdsurface_type::uvsphere,
                   {0, 0.5f, 0}, {0, 35, 45}, {0.5f, 0.5f, 0.5f}),
        make_shape("obj03", materials[4], 4, yshape::stdsurface_type::uvquad,
                   {1.25f, 0.5f, 0}, {0, 35, 45}, {0.5f, 0.5f, 0.5f})};
    if (use_map) {
        std::vector<yapp::environment*> environments;
        materials.push_back(
            make_emission("env", {1, 1, 1}, make_texture("env.hdr")));
        environments.push_back(
            make_env("env", materials[5], {0, 0.5f, 0}, {-1.5f, 0.5f, 0}));
        return make_scene(cameras, shapes, environments);
    } else {
        return make_scene(cameras, concat(shapes, make_simple_lights(true)));
    }
}

yapp::scene* make_rigid_scene(int config) {
    std::vector<yapp::camera*> cameras = {
        make_camera("cam", {5, 5, 5}, {0, 0.5f, 0}, 0.5f, 0),
        make_camera("cam_dof", {5, 5, 5}, {0, 0.5f, 0}, 0.5f, 0.1f)};
    std::vector<yapp::shape*> shapes;

    std::vector<yapp::texture*> textures = {make_texture("grid.png"),
                                            make_texture("checker.png")};

    if (config == 0 || config == 1) {
        std::vector<yapp::material*> materials = {
            make_diffuse("floor", {1, 1, 1}, 0),
            make_plastic("obj", {1, 1, 1}, 0.1f, textures[1])};
        shapes = {
            (config) ? make_shape("floor", materials[0], 2,
                                  yshape::stdsurface_type::uvcube, {0, -2.5, 0},
                                  {30, 0, 0}, {6, 0.5f, 6})
                     : make_shape("floor", materials[0], 4,
                                  yshape::stdsurface_type::uvcube,
                                  {0, -0.5f, 0}, {0, 0, 0}, {6, 0.5f, 6}),
            make_shape("obj01", materials[1], 2,
                       yshape::stdsurface_type::uvcube, {-1.25f, 0.5f, 0},
                       {0, 0, 0}, {0.5f, 0.5f, 0.5f}),
            make_shape("obj02", materials[1], 3,
                       yshape::stdsurface_type::uvspherecube, {0, 1, 0},
                       {0, 0, 0}, {0.5f, 0.5f, 0.5f}),
            make_shape("obj03", materials[1], 2,
                       yshape::stdsurface_type::uvcube, {1.25f, 1.5f, 0},
                       {0, 0, 0}, {0.5f, 0.5f, 0.5f}),
            make_shape("obj11", materials[1], 2,
                       yshape::stdsurface_type::uvcube, {-1.25f, 0.5f, 1.5f},
                       {0, 45, 0}, {0.5f, 0.5f, 0.5f}),
            make_shape("obj12", materials[1], 3,
                       yshape::stdsurface_type::uvspherecube, {0, 1, 1.5f},
                       {45, 0, 0}, {0.5f, 0.5f, 0.5f}),
            make_shape("obj13", materials[1], 2,
                       yshape::stdsurface_type::uvcube, {1.25f, 1.5f, 1.5f},
                       {45, 0, 45}, {0.5f, 0.5f, 0.5f}),
            make_shape("obj21", materials[1], 2,
                       yshape::stdsurface_type::uvcube, {-1.25f, 0.5f, -1.5f},
                       {0, 0, 0}, {0.5f, 0.5f, 0.5f}),
            make_shape("obj22", materials[1], 3,
                       yshape::stdsurface_type::uvspherecube, {0, 1, -1.5f},
                       {22.5, 0, 0}, {0.5f, 0.5f, 0.5f}),
            make_shape("obj23", materials[1], 2,
                       yshape::stdsurface_type::uvcube, {1.25f, 1.5f, -1.5f},
                       {22.5f, 0, 22.5f}, {0.5f, 0.5f, 0.5f})};
    } else if (config == 2) {
        shapes = make_random_rigid_shapes(128, 1, make_random_materials(128));
    } else {
        assert(false);
    }

    shapes.push_back(make_point(
        "light01", make_emission("light01", {100, 100, 100}), {0.7f, 4, 3}));
    shapes.push_back(make_point(
        "light02", make_emission("light02", {100, 100, 100}), {-0.7f, 4, 3}));

    return make_scene(cameras, shapes);
}

int main(int argc, char* argv[]) {
    // command line params
    auto parser = ycmd::make_parser(argc, argv, "make tests");
    auto dirname =
        ycmd::parse_args(parser, "dirname", "directory name", ".", true);
    ycmd::check_parser(parser);

// make directories
#ifndef _MSC_VER
    auto cmd = "mkdir -p " + dirname;
#else
    auto cmd = "mkdir " + dirname;
#endif
    system(cmd.c_str());

    // simple scene ------------------------------
    printf("generating simple scenes ...\n");
    save_scene("basic_pointlight.obj", dirname,
               make_simple_scene(false, false));
    save_scene("simple_pointlight.obj", dirname,
               make_simple_scene(true, false));
    save_scene("simple_arealight.obj", dirname, make_simple_scene(true, true));

    // material scene ------------------------------
    printf("generating mat scenes ...\n");
    save_scene("mat_01_arealights.obj", dirname, make_mat_scene(0, false));
    save_scene("mat_01_envlight.obj", dirname, make_mat_scene(0, true));
    save_scene("mat_02_arealights.obj", dirname, make_mat_scene(1, false));
    save_scene("mat_02_envlight.obj", dirname, make_mat_scene(1, true));
    save_scene("mat_03_arealights.obj", dirname, make_mat_scene(2, false));
    save_scene("mat_03_envlight.obj", dirname, make_mat_scene(2, true));

    // material scene ------------------------------
    printf("generating trans scenes ...\n");
    save_scene("trans_01_arealights.obj", dirname, make_trans_scene(0, false));
    save_scene("trans_01_envlight.obj", dirname, make_trans_scene(0, true));
    //    save_scene("mat_02_arealights.obj", dirname, make_mat_scene(1,
    //    false));
    //    save_scene("mat_02_envlight.obj", dirname, make_mat_scene(1, true));
    //    save_scene("mat_03_arealights.obj", dirname, make_mat_scene(2,
    //    false));
    //    save_scene("mat_03_envlight.obj", dirname, make_mat_scene(2, true));

    // point and lines scene ------------------------------
    printf("generating points and lines scenes ...\n");
    save_scene("points_pointlight.obj", dirname,
               make_pointslines_scene(false, false));
    save_scene("points_arealight.obj", dirname,
               make_pointslines_scene(false, true));
    save_scene("lines_pointlight.obj", dirname,
               make_pointslines_scene(true, false));
    save_scene("lines_arealight.obj", dirname,
               make_pointslines_scene(true, true));

    // random obj scene --------------------------
    printf("generating random shapes scenes ...\n");
    save_scene("random_pointlight.obj", dirname, make_random_scene(32, false));
    save_scene("random_arealight.obj", dirname, make_random_scene(32, true));

    // env scene ------------------------------
    printf("generating envmaps scenes ...\n");
    save_scene("env_shape_const.obj", dirname, make_envmap_scene(true, false));
    save_scene("env_shape_map.obj", dirname, make_envmap_scene(true, true));
    save_scene("env_inf_const.obj", dirname, make_envmap_scene(false, false));
    save_scene("env_inf_map.obj", dirname, make_envmap_scene(false, true));

    // cornell box ------------------------------
    printf("generating cornell box scenes ...\n");

    // save scene
    save_scene("cornell_box.obj", dirname, make_cornell_box_scene());

    // rigid body scenes ------------------------
    printf("generating rigid body scenes ...\n");
    save_scene("rigid_01.obj", dirname, make_rigid_scene(0));
    save_scene("rigid_02.obj", dirname, make_rigid_scene(1));
    // save_scene("rigid_03.obj", dirname, make_rigid_scene(2));

    // textures ---------------------------------
    printf("generating simple textures ...\n");
    save_image("grid.png", dirname, make_grid(512).data(), 512);
    save_image("checker.png", dirname, make_checker(512).data(), 512);
    save_image("rchecker.png", dirname, make_rchecker(512).data(), 512);
    save_image("colored.png", dirname, make_colored(512).data(), 512);
    save_image("rcolored.png", dirname, make_rcolored(512).data(), 512);
    save_image("gamma.png", dirname, make_gammaramp(512).data(), 512);
    save_image_hdr("gamma.hdr", dirname, make_gammarampf(512).data(), 512, 512);
    printf("generating envmaps textures ...\n");
    save_image_hdr("env.hdr", dirname,
                   make_sunsky_hdr(1024, 512, 0.8f, 8,
                                   ym::vec3f{0.2f, 0.2f, 0.2f}, 1 / powf(2, 6),
                                   true)
                       .data(),
                   1024, 512);
    save_image_hdr("env01.hdr", dirname,
                   make_sunsky_hdr(1024, 512, 0.8f, 8,
                                   ym::vec3f{0.2f, 0.2f, 0.2f}, 1 / powf(2, 6),
                                   true)
                       .data(),
                   1024, 512);
}
