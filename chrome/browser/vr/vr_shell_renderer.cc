// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/vr_shell_renderer.h"

#include <math.h>
#include <algorithm>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace {

static constexpr float kHalfSize = 0.5f;

/* clang-format off */
static constexpr float kTexturedQuadVertices[8] = {
    //       x           y
    -kHalfSize,  kHalfSize,
    -kHalfSize, -kHalfSize,
     kHalfSize,  kHalfSize,
     kHalfSize, -kHalfSize,
};

static constexpr GLushort kTexturedQuadIndices[6] = { 0, 1, 2, 1, 3, 2 };

// A rounded rect is subdivided into a number of triangles.
// _______________
// | /    _,-' \ |
// |/_,,-'______\|
// |            /|
// |           / |
// |          /  |
// |         /   |
// |        /    |
// |       /     |
// |      /      |
// |     /       |
// |    /        |
// |   /         |
// |  /          |
// | /           |
// |/____________|
// |\     _,-'' /|
// |_\ ,-'____ /_|
//
// Most of these do not contain an arc. To simplify the rendering of those
// that do, we include a "corner position" attribute. The corner position is
// the distance from the center of the nearest "corner circle". Only those
// triangles containing arcs have a non-zero corner position set. The result
// is that for interior triangles, their corner position is uniformly (0, 0).
// I.e., they are always deemed "inside".
//
// A further complication is that different corner radii will require these
// various triangles to be sized differently relative to one another. We
// would prefer not no continually recreate our vertex buffer, so we include
// another attribute, the "offset scalars". These scalars are only ever 1.0,
// 0.0, or -1.0 and control the addition or subtraction of the horizontal
// and vertical corner offset. This lets the corners of the triangles be
// computed in the vertex shader dynamically. It also happens that the
// texture coordinates can also be easily computed in the vertex shader.
//
// So if the the corner offsets are vr and hr where
//     vr = corner_radius / height;
//     hr = corner_radius / width;
//
// Then the full position is then given by
//   p = (x + osx * hr, y + osy * vr, 0.0, 1.0)
//
// And the full texture coordinate is given by
//   (0.5 + p[0], 0.5 - p[1])
//
static constexpr float kRRectVertices[120] = {
    //       x           y   osx   osy  cpx  cpy
    -kHalfSize,  kHalfSize,  0.0, -1.0, 0.0, 0.0,
    -kHalfSize,  kHalfSize,  1.0,  0.0, 0.0, 0.0,
     kHalfSize,  kHalfSize, -1.0,  0.0, 0.0, 0.0,
     kHalfSize,  kHalfSize,  0.0, -1.0, 0.0, 0.0,
    -kHalfSize, -kHalfSize,  0.0,  1.0, 0.0, 0.0,
     kHalfSize, -kHalfSize,  0.0,  1.0, 0.0, 0.0,
    -kHalfSize, -kHalfSize,  1.0,  0.0, 0.0, 0.0,
     kHalfSize, -kHalfSize, -1.0,  0.0, 0.0, 0.0,
    -kHalfSize,  kHalfSize,  0.0, -1.0, 1.0, 0.0,
    -kHalfSize,  kHalfSize,  0.0,  0.0, 1.0, 1.0,
    -kHalfSize,  kHalfSize,  1.0,  0.0, 0.0, 1.0,
     kHalfSize,  kHalfSize, -1.0,  0.0, 0.0, 1.0,
     kHalfSize,  kHalfSize,  0.0,  0.0, 1.0, 1.0,
     kHalfSize,  kHalfSize,  0.0, -1.0, 1.0, 0.0,
    -kHalfSize, -kHalfSize,  0.0,  0.0, 1.0, 1.0,
    -kHalfSize, -kHalfSize,  0.0,  1.0, 1.0, 0.0,
    -kHalfSize, -kHalfSize,  1.0,  0.0, 0.0, 1.0,
     kHalfSize, -kHalfSize, -1.0,  0.0, 0.0, 1.0,
     kHalfSize, -kHalfSize,  0.0,  1.0, 1.0, 0.0,
     kHalfSize, -kHalfSize,  0.0,  0.0, 1.0, 1.0,
};

static constexpr GLushort kRRectIndices[30] = {
    0,  2,  1,
    0,  3,  2,
    4,  3,  0,
    4,  5,  3,
    4,  6,  5,
    6,  7,  5,
    8,  10, 9,
    11, 13, 12,
    14, 16, 15,
    17, 19, 18,
};

/* clang-format on */

static constexpr int kTexturedQuadPositionDataSize = 2;

static constexpr int kRRectPositionDataSize = 2;
static constexpr size_t kRRectPositionDataOffset = 0;
static constexpr int kRRectOffsetScaleDataSize = 2;
static constexpr size_t kRRectOffsetScaleDataOffset = 2 * sizeof(float);
static constexpr int kRRectCornerPositionDataSize = 2;
static constexpr size_t kRRectCornerPositionDataOffset = 4 * sizeof(float);
static constexpr size_t kRRectDataStride = 6 * sizeof(float);

// Reticle constants
static constexpr float kRingDiameter = 1.0f;
static constexpr float kInnerHole = 0.0f;
static constexpr float kInnerRingEnd = 0.177f;
static constexpr float kInnerRingThickness = 0.14f;
static constexpr float kMidRingEnd = 0.177f;
static constexpr float kMidRingOpacity = 0.22f;
static constexpr float kReticleColor[] = {1.0f, 1.0f, 1.0f, 1.0f};

// Laser constants
static constexpr float kFadeEnd = 0.535;
static constexpr float kFadePoint = 0.5335;
static constexpr float kLaserColor[] = {1.0f, 1.0f, 1.0f, 0.5f};
static constexpr int kLaserDataWidth = 48;
static constexpr int kLaserDataHeight = 1;

// Laser texture data, 48x1 RGBA (not premultiplied alpha).
// TODO(mthiesse): As we add more resources for VR Shell, we should put them
// in Chrome's resource files.
static const unsigned char kLaserData[] =
    "\xff\xff\xff\x01\xff\xff\xff\x02\xbf\xbf\xbf\x04\xcc\xcc\xcc\x05\xdb\xdb"
    "\xdb\x07\xcc\xcc\xcc\x0a\xd8\xd8\xd8\x0d\xd2\xd2\xd2\x11\xce\xce\xce\x15"
    "\xce\xce\xce\x1a\xce\xce\xce\x1f\xcd\xcd\xcd\x24\xc8\xc8\xc8\x2a\xc9\xc9"
    "\xc9\x2f\xc9\xc9\xc9\x34\xc9\xc9\xc9\x39\xc9\xc9\xc9\x3d\xc8\xc8\xc8\x41"
    "\xcb\xcb\xcb\x44\xee\xee\xee\x87\xfa\xfa\xfa\xc8\xf9\xf9\xf9\xc9\xf9\xf9"
    "\xf9\xc9\xfa\xfa\xfa\xc9\xfa\xfa\xfa\xc9\xf9\xf9\xf9\xc9\xf9\xf9\xf9\xc9"
    "\xfa\xfa\xfa\xc8\xee\xee\xee\x87\xcb\xcb\xcb\x44\xc8\xc8\xc8\x41\xc9\xc9"
    "\xc9\x3d\xc9\xc9\xc9\x39\xc9\xc9\xc9\x34\xc9\xc9\xc9\x2f\xc8\xc8\xc8\x2a"
    "\xcd\xcd\xcd\x24\xce\xce\xce\x1f\xce\xce\xce\x1a\xce\xce\xce\x15\xd2\xd2"
    "\xd2\x11\xd8\xd8\xd8\x0d\xcc\xcc\xcc\x0a\xdb\xdb\xdb\x07\xcc\xcc\xcc\x05"
    "\xbf\xbf\xbf\x04\xff\xff\xff\x02\xff\xff\xff\x01";

#define SHADER(Src) #Src
#define OEIE_SHADER(Src) "#extension GL_OES_EGL_image_external : require\n" #Src
#define VOID_OFFSET(x) reinterpret_cast<void*>(x)

const char* GetShaderSource(vr::ShaderID shader) {
  switch (shader) {
    case vr::ShaderID::RETICLE_VERTEX_SHADER:
    case vr::ShaderID::LASER_VERTEX_SHADER:
    case vr::ShaderID::TEXTURED_QUAD_VERTEX_SHADER:
      return SHADER(
          /* clang-format off */
          precision mediump float;
          uniform mat4 u_ModelViewProjMatrix;
          attribute vec4 a_Position;
          varying vec2 v_TexCoordinate;

          void main() {
            v_TexCoordinate = vec2(0.5 + a_Position[0], 0.5 - a_Position[1]);
            gl_Position = u_ModelViewProjMatrix * a_Position;
          }
          /* clang-format on */);
    case vr::ShaderID::CONTROLLER_VERTEX_SHADER:
      return SHADER(
          /* clang-format off */
          precision mediump float;
          uniform mat4 u_ModelViewProjMatrix;
          attribute vec4 a_Position;
          attribute vec2 a_TexCoordinate;
          varying vec2 v_TexCoordinate;

          void main() {
            v_TexCoordinate = a_TexCoordinate;
            gl_Position = u_ModelViewProjMatrix * a_Position;
          }
          /* clang-format on */);
    case vr::ShaderID::GRADIENT_QUAD_VERTEX_SHADER:
    case vr::ShaderID::GRADIENT_GRID_VERTEX_SHADER:
      return SHADER(
          /* clang-format off */
          precision mediump float;
          uniform mat4 u_ModelViewProjMatrix;
          uniform float u_SceneRadius;
          attribute vec4 a_Position;
          varying vec2 v_GridPosition;

          void main() {
            v_GridPosition = a_Position.xy / u_SceneRadius;
            gl_Position = u_ModelViewProjMatrix * a_Position;
          }
          /* clang-format on */);
    case vr::ShaderID::EXTERNAL_TEXTURED_QUAD_VERTEX_SHADER:
      return SHADER(
          /* clang-format off */
          precision mediump float;
          uniform mat4 u_ModelViewProjMatrix;
          uniform vec2 u_CornerOffset;
          attribute vec4 a_Position;
          attribute vec2 a_CornerPosition;
          attribute vec2 a_OffsetScale;
          varying vec2 v_TexCoordinate;
          varying vec2 v_CornerPosition;

          void main() {
            v_CornerPosition = a_CornerPosition;
            vec4 position = vec4(
                a_Position[0] + u_CornerOffset[0] * a_OffsetScale[0],
                a_Position[1] + u_CornerOffset[1] * a_OffsetScale[1],
                a_Position[2],
                a_Position[3]);
            v_TexCoordinate = vec2(0.5 + position[0], 0.5 - position[1]);
            gl_Position = u_ModelViewProjMatrix * position;
          }
          /* clang-format on */);
    case vr::ShaderID::EXTERNAL_TEXTURED_QUAD_FRAGMENT_SHADER:
      return OEIE_SHADER(
          /* clang-format off */
          precision highp float;
          uniform samplerExternalOES u_Texture;
          varying vec2 v_TexCoordinate;
          varying vec2 v_CornerPosition;
          uniform float u_CornerScale;
          uniform mediump float u_Opacity;

          void main() {
            lowp vec4 color = texture2D(u_Texture, v_TexCoordinate);
            float mask = smoothstep(1.0 + u_CornerScale,
                                    1.0 - u_CornerScale,
                                    length(v_CornerPosition));
            gl_FragColor = color * u_Opacity * mask;
          }
          /* clang-format on */);
    case vr::ShaderID::TEXTURED_QUAD_FRAGMENT_SHADER:
      return SHADER(
          /* clang-format off */
          precision highp float;
          uniform sampler2D u_Texture;
          uniform vec4 u_CopyRect;  // rectangle
          varying vec2 v_TexCoordinate;
          uniform lowp vec4 color;
          uniform mediump float opacity;

          void main() {
            vec2 scaledTex =
                vec2(u_CopyRect[0] + v_TexCoordinate.x * u_CopyRect[2],
                     u_CopyRect[1] + v_TexCoordinate.y * u_CopyRect[3]);
            lowp vec4 color = texture2D(u_Texture, scaledTex);
            gl_FragColor = color * opacity;
          }
          /* clang-format on */);
    case vr::ShaderID::WEBVR_VERTEX_SHADER:
      return SHADER(
          /* clang-format off */
          precision mediump float;
          attribute vec4 a_Position;
          varying vec2 v_TexCoordinate;

          void main() {
            v_TexCoordinate = vec2(0.5 + a_Position[0], 0.5 - a_Position[1]);
            gl_Position = vec4(a_Position.xyz * 2.0, 1.0);
          }
          /* clang-format on */);
    case vr::ShaderID::WEBVR_FRAGMENT_SHADER:
      return OEIE_SHADER(
          /* clang-format off */
          precision highp float;
          uniform samplerExternalOES u_Texture;
          varying vec2 v_TexCoordinate;

          void main() {
            gl_FragColor = texture2D(u_Texture, v_TexCoordinate);
          }
          /* clang-format on */);
    case vr::ShaderID::RETICLE_FRAGMENT_SHADER:
      return SHADER(
          /* clang-format off */
          precision mediump float;
          varying mediump vec2 v_TexCoordinate;
          uniform lowp vec4 color;
          uniform mediump float ring_diameter;
          uniform mediump float inner_hole;
          uniform mediump float inner_ring_end;
          uniform mediump float inner_ring_thickness;
          uniform mediump float mid_ring_end;
          uniform mediump float mid_ring_opacity;

          void main() {
            mediump float r = length(v_TexCoordinate - vec2(0.5, 0.5));
            mediump float color_radius = inner_ring_end * ring_diameter;
            mediump float color_feather_radius =
                inner_ring_thickness * ring_diameter;
            mediump float hole_radius =
                inner_hole * ring_diameter - color_feather_radius;
            mediump float color1 = clamp(
                1.0 - (r - color_radius) / color_feather_radius, 0.0, 1.0);
            mediump float hole_alpha =
                clamp((r - hole_radius) / color_feather_radius, 0.0, 1.0);

            mediump float black_radius = mid_ring_end * ring_diameter;
            mediump float black_feather =
                1.0 / (ring_diameter * 0.5 - black_radius);
            mediump float black_alpha_factor =
                mid_ring_opacity * (1.0 - (r - black_radius) * black_feather);
            mediump float alpha = clamp(
                min(hole_alpha, max(color1, black_alpha_factor)), 0.0, 1.0);
            lowp vec3 color_rgb = color1 * color.xyz;
            gl_FragColor = vec4(color_rgb * color.w * alpha, color.w * alpha);
          }
          /* clang-format on */);
    case vr::ShaderID::LASER_FRAGMENT_SHADER:
      return SHADER(
          /* clang-format off */
          varying mediump vec2 v_TexCoordinate;
          uniform sampler2D texture_unit;
          uniform lowp vec4 color;
          uniform mediump float fade_point;
          uniform mediump float fade_end;
          uniform mediump float u_Opacity;

          void main() {
            mediump vec2 uv = v_TexCoordinate;
            mediump float front_fade_factor = 1.0 -
                clamp(1.0 - (uv.y - fade_point) / (1.0 - fade_point), 0.0, 1.0);
            mediump float back_fade_factor =
                clamp((uv.y - fade_point) / (fade_end - fade_point), 0.0, 1.0);
            mediump float total_fade = front_fade_factor * back_fade_factor;
            lowp vec4 texture_color = texture2D(texture_unit, uv);
            lowp vec4 final_color = color * texture_color;
            lowp float final_opacity = final_color.w * total_fade * u_Opacity;
            gl_FragColor = vec4(final_color.xyz * final_opacity, final_opacity);
          }
          /* clang-format on */);
    case vr::ShaderID::GRADIENT_QUAD_FRAGMENT_SHADER:
      return SHADER(
          /* clang-format off */
          precision lowp float;
          varying vec2 v_GridPosition;
          uniform vec4 u_CenterColor;
          uniform vec4 u_EdgeColor;
          uniform mediump float u_Opacity;

          void main() {
            float edgeColorWeight = clamp(length(v_GridPosition), 0.0, 1.0);
            float centerColorWeight = 1.0 - edgeColorWeight;
            vec4 color = u_CenterColor * centerColorWeight +
                u_EdgeColor * edgeColorWeight;
            gl_FragColor = vec4(color.xyz * color.w * u_Opacity,
                                color.w * u_Opacity);
          }
          /* clang-format on */);
    case vr::ShaderID::GRADIENT_GRID_FRAGMENT_SHADER:
      return SHADER(
          /* clang-format off */
          precision lowp float;
          varying vec2 v_GridPosition;
          uniform vec4 u_CenterColor;
          uniform vec4 u_EdgeColor;
          uniform vec4 u_GridColor;
          uniform mediump float u_Opacity;
          uniform float u_LinesCount;

          void main() {
            float edgeColorWeight = clamp(length(v_GridPosition), 0.0, 1.0);
            float centerColorWeight = 1.0 - edgeColorWeight;
            vec4 bg_color = u_CenterColor * centerColorWeight +
                u_EdgeColor * edgeColorWeight;
            bg_color = vec4(bg_color.xyz * bg_color.w, bg_color.w);
            float linesCountF = u_LinesCount * 0.5;
            float pos_x = abs(v_GridPosition.x) * linesCountF;
            float pos_y = abs(v_GridPosition.y) * linesCountF;
            float diff_x = abs(pos_x - floor(pos_x + 0.5));
            float diff_y = abs(pos_y - floor(pos_y + 0.5));
            float diff = min(diff_x, diff_y);
            if (diff < 0.01) {
              float opacity = 1.0 - diff / 0.01;
              opacity = opacity * opacity * centerColorWeight * u_GridColor.w;
              vec3 grid_color = u_GridColor.xyz * opacity;
              gl_FragColor = vec4(
                  grid_color.xyz + bg_color.xyz * (1.0 - opacity),
                  opacity + bg_color.w * (1.0 - opacity));
            } else {
              gl_FragColor = bg_color;
            }
          }
          /* clang-format on */);
    case vr::ShaderID::CONTROLLER_FRAGMENT_SHADER:
      return SHADER(
          /* clang-format off */
          precision mediump float;
          uniform sampler2D u_texture;
          varying vec2 v_TexCoordinate;
          uniform mediump float u_Opacity;

          void main() {
            lowp vec4 texture_color = texture2D(u_texture, v_TexCoordinate);
            gl_FragColor = texture_color * u_Opacity;
          }
          /* clang-format on */);
    default:
      LOG(ERROR) << "Shader source requested for unknown shader";
      return "";
  }
}

void SetColorUniform(GLuint handle, SkColor c) {
  glUniform4f(handle, SkColorGetR(c) / 255.0, SkColorGetG(c) / 255.0,
              SkColorGetB(c) / 255.0, SkColorGetA(c) / 255.0);
}

}  // namespace

namespace vr {

BaseRenderer::BaseRenderer(ShaderID vertex_id, ShaderID fragment_id) {
  std::string error;
  GLuint vertex_shader_handle =
      CompileShader(GL_VERTEX_SHADER, GetShaderSource(vertex_id), error);
  CHECK(vertex_shader_handle) << error;

  GLuint fragment_shader_handle =
      CompileShader(GL_FRAGMENT_SHADER, GetShaderSource(fragment_id), error);
  CHECK(fragment_shader_handle) << error;

  program_handle_ =
      CreateAndLinkProgram(vertex_shader_handle, fragment_shader_handle, error);
  CHECK(program_handle_) << error;

  // Once the program is linked the shader objects are no longer needed
  glDeleteShader(vertex_shader_handle);
  glDeleteShader(fragment_shader_handle);

  position_handle_ = glGetAttribLocation(program_handle_, "a_Position");
}

BaseRenderer::~BaseRenderer() = default;

BaseQuadRenderer::BaseQuadRenderer(ShaderID vertex_id, ShaderID fragment_id)
    : BaseRenderer(vertex_id, fragment_id) {}

BaseQuadRenderer::~BaseQuadRenderer() = default;

void BaseQuadRenderer::PrepareToDraw(GLuint view_proj_matrix_handle,
                                     const gfx::Transform& view_proj_matrix) {
  glUseProgram(program_handle_);

  // Pass in model view project matrix.
  glUniformMatrix4fv(view_proj_matrix_handle, 1, false,
                     MatrixToGLArray(view_proj_matrix).data());

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, kTexturedQuadPositionDataSize,
                        GL_FLOAT, false, 0, 0);
  glEnableVertexAttribArray(position_handle_);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

GLuint BaseQuadRenderer::vertex_buffer_ = 0;
GLuint BaseQuadRenderer::index_buffer_ = 0;

void BaseQuadRenderer::SetVertexBuffer() {
  GLuint buffers[2];
  glGenBuffersARB(2, buffers);
  vertex_buffer_ = buffers[0];
  index_buffer_ = buffers[1];

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER,
               arraysize(kTexturedQuadVertices) * sizeof(float),
               kTexturedQuadVertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               arraysize(kTexturedQuadIndices) * sizeof(GLushort),
               kTexturedQuadIndices, GL_STATIC_DRAW);
}

GLuint ExternalTexturedQuadRenderer::vertex_buffer_ = 0;
GLuint ExternalTexturedQuadRenderer::index_buffer_ = 0;

void ExternalTexturedQuadRenderer::SetVertexBuffer() {
  GLuint buffers[2];
  glGenBuffersARB(2, buffers);
  vertex_buffer_ = buffers[0];
  index_buffer_ = buffers[1];

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, arraysize(kRRectVertices) * sizeof(float),
               kRRectVertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               arraysize(kRRectIndices) * sizeof(GLushort), kRRectIndices,
               GL_STATIC_DRAW);
}

ExternalTexturedQuadRenderer::ExternalTexturedQuadRenderer()
    : BaseRenderer(EXTERNAL_TEXTURED_QUAD_VERTEX_SHADER,
                   EXTERNAL_TEXTURED_QUAD_FRAGMENT_SHADER) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  corner_offset_handle_ =
      glGetUniformLocation(program_handle_, "u_CornerOffset");
  corner_scale_handle_ = glGetUniformLocation(program_handle_, "u_CornerScale");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
  texture_handle_ = glGetUniformLocation(program_handle_, "u_Texture");

  corner_position_handle_ =
      glGetAttribLocation(program_handle_, "a_CornerPosition");
  offset_scale_handle_ = glGetAttribLocation(program_handle_, "a_OffsetScale");
}

void ExternalTexturedQuadRenderer::Draw(int texture_data_handle,
                                        const gfx::Transform& view_proj_matrix,
                                        const gfx::Size& surface_size,
                                        const gfx::SizeF& element_size,
                                        float opacity,
                                        float corner_radius) {
  if (element_size.IsEmpty())
    return;

  glUseProgram(program_handle_);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // Pass in model view project matrix.
  glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                     MatrixToGLArray(view_proj_matrix).data());

  gfx::Point3F top_left(-0.5, 0.5, 0.0);
  gfx::Point3F top_right(0.5, 0.5, 0.0);
  view_proj_matrix.TransformPoint(&top_left);
  view_proj_matrix.TransformPoint(&top_right);
  gfx::Vector3dF top_vector = top_right - top_left;
  float physical_width = top_vector.Length();
  physical_width *= corner_radius / element_size.width();
  physical_width *= 0.5 * surface_size.width();

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, kRRectPositionDataSize, GL_FLOAT,
                        false, kRRectDataStride,
                        VOID_OFFSET(kRRectPositionDataOffset));
  glEnableVertexAttribArray(position_handle_);

  // Set up offset scale attribute.
  glVertexAttribPointer(offset_scale_handle_, kRRectOffsetScaleDataSize,
                        GL_FLOAT, false, kRRectDataStride,
                        VOID_OFFSET(kRRectOffsetScaleDataOffset));
  glEnableVertexAttribArray(offset_scale_handle_);

  // Set up corner position attribute.
  glVertexAttribPointer(corner_position_handle_, kRRectCornerPositionDataSize,
                        GL_FLOAT, false, kRRectDataStride,
                        VOID_OFFSET(kRRectCornerPositionDataOffset));
  glEnableVertexAttribArray(corner_position_handle_);

  // Link texture data with texture unit.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_data_handle);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Set up uniforms.
  glUniform1i(texture_handle_, 0);
  glUniform1f(opacity_handle_, opacity);
  glUniform1f(corner_scale_handle_, 1.0f / physical_width);
  glUniform2f(corner_offset_handle_, corner_radius / element_size.width(),
              corner_radius / element_size.height());

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, arraysize(kRRectIndices), GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
  glDisableVertexAttribArray(offset_scale_handle_);
  glDisableVertexAttribArray(corner_position_handle_);
}

ExternalTexturedQuadRenderer::~ExternalTexturedQuadRenderer() = default;

TexturedQuadRenderer::TexturedQuadRenderer()
    : BaseQuadRenderer(TEXTURED_QUAD_VERTEX_SHADER,
                       TEXTURED_QUAD_FRAGMENT_SHADER) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  tex_uniform_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
  copy_rect_uniform_handle_ =
      glGetUniformLocation(program_handle_, "u_CopyRect");
  opacity_handle_ = glGetUniformLocation(program_handle_, "opacity");
}

void TexturedQuadRenderer::AddQuad(int texture_data_handle,
                                   const gfx::Transform& view_proj_matrix,
                                   const gfx::RectF& copy_rect,
                                   float opacity) {
  SkiaQuad quad;
  quad.texture_data_handle = texture_data_handle;
  quad.view_proj_matrix = view_proj_matrix;
  quad.copy_rect = {copy_rect.x(), copy_rect.y(), copy_rect.width(),
                    copy_rect.height()};
  quad.opacity = opacity;
  quad_queue_.push(quad);
}

void TexturedQuadRenderer::Flush() {
  if (quad_queue_.empty())
    return;

  int last_texture = 0;
  float last_opacity = -1.0f;

  // Set up GL state that doesn't change between draw calls.
  glUseProgram(program_handle_);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, kTexturedQuadPositionDataSize,
                        GL_FLOAT, false, 0, 0);
  glEnableVertexAttribArray(position_handle_);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // Link texture data with texture unit.
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(tex_uniform_handle_, 0);

  // TODO(bajones): This should eventually be changed to use instancing so that
  // the entire queue can be processed in one draw call. For now this still
  // significantly reduces the amount of state changes made per draw.
  while (!quad_queue_.empty()) {
    const SkiaQuad& quad = quad_queue_.front();

    // Only change texture ID or opacity when they differ between quads.
    if (last_texture != quad.texture_data_handle) {
      last_texture = quad.texture_data_handle;
      glBindTexture(GL_TEXTURE_2D, last_texture);
    }

    if (last_opacity != quad.opacity) {
      last_opacity = quad.opacity;
      glUniform1f(opacity_handle_, last_opacity);
    }

    // Pass in model view project matrix.
    glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                       MatrixToGLArray(quad.view_proj_matrix).data());

    // Pass in the copy rect.
    glUniform4fv(copy_rect_uniform_handle_, 1,
                 reinterpret_cast<const float*>(&quad.copy_rect));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
    glDrawElements(GL_TRIANGLES, arraysize(kTexturedQuadIndices),
                   GL_UNSIGNED_SHORT, 0);

    quad_queue_.pop();
  }

  glDisableVertexAttribArray(position_handle_);
}

TexturedQuadRenderer::~TexturedQuadRenderer() = default;

WebVrRenderer::WebVrRenderer()
    : BaseQuadRenderer(WEBVR_VERTEX_SHADER, WEBVR_FRAGMENT_SHADER) {
  tex_uniform_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
}

// Draw the stereo WebVR frame
void WebVrRenderer::Draw(int texture_handle) {
  glUseProgram(program_handle_);

  // Bind vertex attributes
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  // Set up position attribute.
  glVertexAttribPointer(position_handle_, kTexturedQuadPositionDataSize,
                        GL_FLOAT, false, 0, 0);
  glEnableVertexAttribArray(position_handle_);

  // Bind texture. This is a 1:1 pixel copy since the source surface
  // and renderbuffer destination size are resized to match, so use
  // GL_NEAREST.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture_handle);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glUniform1i(tex_uniform_handle_, 0);

  // Blit texture to buffer
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, arraysize(kTexturedQuadIndices),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

// Note that we don't explicitly delete gl objects here, they're deleted
// automatically when we call ShutdownGL, and deleting them here leads to
// segfaults.
WebVrRenderer::~WebVrRenderer() = default;

ReticleRenderer::ReticleRenderer()
    : BaseQuadRenderer(RETICLE_VERTEX_SHADER, RETICLE_FRAGMENT_SHADER) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  color_handle_ = glGetUniformLocation(program_handle_, "color");
  ring_diameter_handle_ =
      glGetUniformLocation(program_handle_, "ring_diameter");
  inner_hole_handle_ = glGetUniformLocation(program_handle_, "inner_hole");
  inner_ring_end_handle_ =
      glGetUniformLocation(program_handle_, "inner_ring_end");
  inner_ring_thickness_handle_ =
      glGetUniformLocation(program_handle_, "inner_ring_thickness");
  mid_ring_end_handle_ = glGetUniformLocation(program_handle_, "mid_ring_end");
  mid_ring_opacity_handle_ =
      glGetUniformLocation(program_handle_, "mid_ring_opacity");
}

void ReticleRenderer::Draw(const gfx::Transform& view_proj_matrix) {
  PrepareToDraw(model_view_proj_matrix_handle_, view_proj_matrix);

  glUniform4f(color_handle_, kReticleColor[0], kReticleColor[1],
              kReticleColor[2], kReticleColor[3]);
  glUniform1f(ring_diameter_handle_, kRingDiameter);
  glUniform1f(inner_hole_handle_, kInnerHole);
  glUniform1f(inner_ring_end_handle_, kInnerRingEnd);
  glUniform1f(inner_ring_thickness_handle_, kInnerRingThickness);
  glUniform1f(mid_ring_end_handle_, kMidRingEnd);
  glUniform1f(mid_ring_opacity_handle_, kMidRingOpacity);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, arraysize(kTexturedQuadIndices),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

ReticleRenderer::~ReticleRenderer() = default;

LaserRenderer::LaserRenderer()
    : BaseQuadRenderer(LASER_VERTEX_SHADER, LASER_FRAGMENT_SHADER) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  texture_unit_handle_ = glGetUniformLocation(program_handle_, "texture_unit");
  color_handle_ = glGetUniformLocation(program_handle_, "color");
  fade_point_handle_ = glGetUniformLocation(program_handle_, "fade_point");
  fade_end_handle_ = glGetUniformLocation(program_handle_, "fade_end");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");

  glGenTextures(1, &texture_data_handle_);
  glBindTexture(GL_TEXTURE_2D, texture_data_handle_);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kLaserDataWidth, kLaserDataHeight, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, kLaserData);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void LaserRenderer::Draw(float opacity,
                         const gfx::Transform& view_proj_matrix) {
  PrepareToDraw(model_view_proj_matrix_handle_, view_proj_matrix);

  // Link texture data with texture unit.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_data_handle_);

  glUniform1i(texture_unit_handle_, 0);
  glUniform4f(color_handle_, kLaserColor[0], kLaserColor[1], kLaserColor[2],
              kLaserColor[3]);
  glUniform1f(fade_point_handle_, kFadePoint);
  glUniform1f(fade_end_handle_, kFadeEnd);
  glUniform1f(opacity_handle_, opacity);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, arraysize(kTexturedQuadIndices),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

LaserRenderer::~LaserRenderer() = default;

ControllerRenderer::ControllerRenderer()
    : BaseRenderer(CONTROLLER_VERTEX_SHADER, CONTROLLER_FRAGMENT_SHADER),
      texture_handles_(VrControllerModel::STATE_COUNT) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  tex_coord_handle_ = glGetAttribLocation(program_handle_, "a_TexCoordinate");
  tex_uniform_handle_ = glGetUniformLocation(program_handle_, "u_Texture");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
}

ControllerRenderer::~ControllerRenderer() = default;

void ControllerRenderer::SetUp(std::unique_ptr<VrControllerModel> model) {
  TRACE_EVENT0("gpu", "ControllerRenderer::SetUp");
  glGenBuffersARB(1, &indices_buffer_);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buffer_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, model->IndicesBufferSize(),
               model->IndicesBuffer(), GL_STATIC_DRAW);

  glGenBuffersARB(1, &vertex_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, model->ElementsBufferSize(),
               model->ElementsBuffer(), GL_STATIC_DRAW);

  glGenTextures(VrControllerModel::STATE_COUNT, texture_handles_.data());
  for (int i = 0; i < VrControllerModel::STATE_COUNT; i++) {
    sk_sp<SkImage> texture = model->GetTexture(i);
    SkPixmap pixmap;
    if (!texture->peekPixels(&pixmap)) {
      LOG(ERROR) << "Failed to read controller texture pixels";
      continue;
    }
    glBindTexture(GL_TEXTURE_2D, texture_handles_[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pixmap.width(), pixmap.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, pixmap.addr());
  }

  const vr::gltf::Accessor* accessor = model->PositionAccessor();
  position_components_ = vr::gltf::GetTypeComponents(accessor->type);
  position_type_ = accessor->component_type;
  position_stride_ = accessor->byte_stride;
  position_offset_ = VOID_OFFSET(accessor->byte_offset);

  accessor = model->TextureCoordinateAccessor();
  tex_coord_components_ = vr::gltf::GetTypeComponents(accessor->type);
  tex_coord_type_ = accessor->component_type;
  tex_coord_stride_ = accessor->byte_stride;
  tex_coord_offset_ = VOID_OFFSET(accessor->byte_offset);

  draw_mode_ = model->DrawMode();
  accessor = model->IndicesAccessor();
  indices_count_ = accessor->count;
  indices_type_ = accessor->component_type;
  indices_offset_ = VOID_OFFSET(accessor->byte_offset);
  setup_ = true;
}

void ControllerRenderer::Draw(VrControllerModel::State state,
                              float opacity,
                              const gfx::Transform& view_proj_matrix) {
  glUseProgram(program_handle_);

  glUniform1f(opacity_handle_, opacity);

  glUniformMatrix4fv(model_view_proj_matrix_handle_, 1, false,
                     MatrixToGLArray(view_proj_matrix).data());

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  glVertexAttribPointer(position_handle_, position_components_, position_type_,
                        GL_FALSE, position_stride_, position_offset_);
  glEnableVertexAttribArray(position_handle_);

  glVertexAttribPointer(tex_coord_handle_, tex_coord_components_,
                        tex_coord_type_, GL_FALSE, tex_coord_stride_,
                        tex_coord_offset_);
  glEnableVertexAttribArray(tex_coord_handle_);

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_buffer_);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_handles_[state]);
  glUniform1i(tex_uniform_handle_, 0);

  glDrawElements(draw_mode_, indices_count_, indices_type_, indices_offset_);
}

GradientQuadRenderer::GradientQuadRenderer()
    : BaseQuadRenderer(GRADIENT_QUAD_VERTEX_SHADER,
                       GRADIENT_QUAD_FRAGMENT_SHADER) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  scene_radius_handle_ = glGetUniformLocation(program_handle_, "u_SceneRadius");
  center_color_handle_ = glGetUniformLocation(program_handle_, "u_CenterColor");
  edge_color_handle_ = glGetUniformLocation(program_handle_, "u_EdgeColor");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
}

void GradientQuadRenderer::Draw(const gfx::Transform& view_proj_matrix,
                                SkColor edge_color,
                                SkColor center_color,
                                float opacity) {
  PrepareToDraw(model_view_proj_matrix_handle_, view_proj_matrix);

  // Tell shader the grid size so that it can calculate the fading.
  glUniform1f(scene_radius_handle_, kHalfSize);

  // Set the edge color to the fog color so that it seems to fade out.
  SetColorUniform(edge_color_handle_, edge_color);
  SetColorUniform(center_color_handle_, center_color);
  glUniform1f(opacity_handle_, opacity);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, arraysize(kTexturedQuadIndices),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

GradientQuadRenderer::~GradientQuadRenderer() = default;

GradientGridRenderer::GradientGridRenderer()
    : BaseQuadRenderer(GRADIENT_QUAD_VERTEX_SHADER,
                       GRADIENT_GRID_FRAGMENT_SHADER) {
  model_view_proj_matrix_handle_ =
      glGetUniformLocation(program_handle_, "u_ModelViewProjMatrix");
  scene_radius_handle_ = glGetUniformLocation(program_handle_, "u_SceneRadius");
  center_color_handle_ = glGetUniformLocation(program_handle_, "u_CenterColor");
  edge_color_handle_ = glGetUniformLocation(program_handle_, "u_EdgeColor");
  grid_color_handle_ = glGetUniformLocation(program_handle_, "u_GridColor");
  opacity_handle_ = glGetUniformLocation(program_handle_, "u_Opacity");
  lines_count_handle_ = glGetUniformLocation(program_handle_, "u_LinesCount");
}

void GradientGridRenderer::Draw(const gfx::Transform& view_proj_matrix,
                                SkColor edge_color,
                                SkColor center_color,
                                SkColor grid_color,
                                int gridline_count,
                                float opacity) {
  PrepareToDraw(model_view_proj_matrix_handle_, view_proj_matrix);

  // Tell shader the grid size so that it can calculate the fading.
  glUniform1f(scene_radius_handle_, kHalfSize);
  glUniform1f(lines_count_handle_, static_cast<float>(gridline_count));

  // Set the edge color to the fog color so that it seems to fade out.
  SetColorUniform(edge_color_handle_, edge_color);
  SetColorUniform(center_color_handle_, center_color);
  SetColorUniform(grid_color_handle_, grid_color);
  glUniform1f(opacity_handle_, opacity);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_);
  glDrawElements(GL_TRIANGLES, arraysize(kTexturedQuadIndices),
                 GL_UNSIGNED_SHORT, 0);

  glDisableVertexAttribArray(position_handle_);
}

GradientGridRenderer::~GradientGridRenderer() = default;

VrShellRenderer::VrShellRenderer()
    : external_textured_quad_renderer_(
          base::MakeUnique<ExternalTexturedQuadRenderer>()),
      textured_quad_renderer_(base::MakeUnique<TexturedQuadRenderer>()),
      webvr_renderer_(base::MakeUnique<WebVrRenderer>()),
      reticle_renderer_(base::MakeUnique<ReticleRenderer>()),
      laser_renderer_(base::MakeUnique<LaserRenderer>()),
      controller_renderer_(base::MakeUnique<ControllerRenderer>()),
      gradient_quad_renderer_(base::MakeUnique<GradientQuadRenderer>()),
      gradient_grid_renderer_(base::MakeUnique<GradientGridRenderer>()) {
  BaseQuadRenderer::SetVertexBuffer();
  ExternalTexturedQuadRenderer::SetVertexBuffer();
}

VrShellRenderer::~VrShellRenderer() = default;

void VrShellRenderer::DrawTexturedQuad(int texture_data_handle,
                                       const gfx::Transform& view_proj_matrix,
                                       const gfx::RectF& copy_rect,
                                       float opacity) {
  GetTexturedQuadRenderer()->AddQuad(texture_data_handle, view_proj_matrix,
                                     copy_rect, opacity);
}

void VrShellRenderer::DrawGradientQuad(const gfx::Transform& view_proj_matrix,
                                       const SkColor edge_color,
                                       const SkColor center_color,
                                       float opacity) {
  GetGradientQuadRenderer()->Draw(view_proj_matrix, edge_color, center_color,
                                  opacity);
}

ExternalTexturedQuadRenderer*
VrShellRenderer::GetExternalTexturedQuadRenderer() {
  Flush();
  return external_textured_quad_renderer_.get();
}

TexturedQuadRenderer* VrShellRenderer::GetTexturedQuadRenderer() {
  return textured_quad_renderer_.get();
}

WebVrRenderer* VrShellRenderer::GetWebVrRenderer() {
  Flush();
  return webvr_renderer_.get();
}

ReticleRenderer* VrShellRenderer::GetReticleRenderer() {
  Flush();
  return reticle_renderer_.get();
}

LaserRenderer* VrShellRenderer::GetLaserRenderer() {
  Flush();
  return laser_renderer_.get();
}

ControllerRenderer* VrShellRenderer::GetControllerRenderer() {
  Flush();
  return controller_renderer_.get();
}

GradientQuadRenderer* VrShellRenderer::GetGradientQuadRenderer() {
  Flush();
  return gradient_quad_renderer_.get();
}

GradientGridRenderer* VrShellRenderer::GetGradientGridRenderer() {
  Flush();
  return gradient_grid_renderer_.get();
}

void VrShellRenderer::Flush() {
  textured_quad_renderer_->Flush();
}

}  // namespace vr
