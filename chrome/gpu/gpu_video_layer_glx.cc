// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/gpu_video_layer_glx.h"

#include "app/gfx/gl/gl_bindings.h"
#include "chrome/common/gpu_messages.h"
#include "chrome/gpu/gpu_thread.h"
#include "chrome/gpu/gpu_view_x.h"

// Handy constants for addressing YV12 data.
static const int kYUVPlanes = 3;
static const int kYPlane = 0;
static const int kUPlane = 1;
static const int kVPlane = 2;

// Buffer size for shader compile errors.
static const unsigned int kErrorSize = 4096;

// Matrix used for the YUV to RGB conversion.
static const float kYUV2RGB[9] = {
  1.f, 0.f, 1.403f,
  1.f, -.344f, -.714f,
  1.f, 1.772f, 0.f,
};

// Texture coordinates mapping the entire texture.
static const float kTextureCoords[8] = {
  0, 0,
  0, 1,
  1, 0,
  1, 1,
};

#define I915_WORKAROUND

// Pass-through vertex shader.
static const char kVertexShader[] =
    "varying vec2 interp_tc;\n"
    "\n"
    "attribute vec4 in_pos;\n"
    "attribute vec2 in_tc;\n"
    "\n"
    "void main() {\n"
#if defined(I915_WORKAROUND)
    "  gl_TexCoord[0].st = in_tc;\n"
#else
    "  interp_tc = in_tc;\n"
#endif
    "  gl_Position = in_pos;\n"
    "}\n";

// YUV to RGB pixel shader. Loads a pixel from each plane and pass through the
// matrix.
static const char kFragmentShader[] =
    "varying vec2 interp_tc;\n"
    "\n"
    "uniform sampler2D y_tex;\n"
    "uniform sampler2D u_tex;\n"
    "uniform sampler2D v_tex;\n"
    "uniform mat3 yuv2rgb;\n"
    "\n"
    "void main() {\n"
#if defined(I915_WORKAROUND)
    "  float y = texture2D(y_tex, gl_TexCoord[0].st).x;\n"
    "  float u = texture2D(u_tex, gl_TexCoord[0].st).r - .5;\n"
    "  float v = texture2D(v_tex, gl_TexCoord[0].st).r - .5;\n"
    "  float r = y + v * 1.403;\n"
    "  float g = y - u * 0.344 - v * 0.714;\n"
    "  float b = y + u * 1.772;\n"
    "  gl_FragColor = vec4(r, g, b, 1);\n"
#else
    "  float y = texture2D(y_tex, interp_tc).x;\n"
    "  float u = texture2D(u_tex, interp_tc).r - .5;\n"
    "  float v = texture2D(v_tex, interp_tc).r - .5;\n"
    "  vec3 rgb = yuv2rgb * vec3(y, u, v);\n"
    "  gl_FragColor = vec4(rgb, 1);\n"
#endif
    "}\n";


// Assume that somewhere along the line, someone will do width * height * 4
// with signed numbers. If the maximum value is 2**31, then 2**31 / 4 =
// 2**29 and floor(sqrt(2**29)) = 23170.

// Max height and width for layers
static const int kMaxVideoLayerSize = 23170;

GpuVideoLayerGLX::GpuVideoLayerGLX(GpuViewX* view,
                                   GpuThread* gpu_thread,
                                   int32 routing_id,
                                   const gfx::Size& size)
    : view_(view),
      gpu_thread_(gpu_thread),
      routing_id_(routing_id),
      native_size_(size),
      program_(0) {
  memset(textures_, 0, sizeof(textures_));

  // Load identity vertices.
  gfx::Rect identity(0, 0, 1, 1);
  CalculateVertices(identity.size(), identity, target_vertices_);

  gpu_thread_->AddRoute(routing_id_, this);

  view_->BindContext();  // Must do this before issuing OpenGl.

  // TODO(apatrick): These functions are not available in GLES2.
  // glMatrixMode(GL_MODELVIEW);

  // Create 3 textures, one for each plane, and bind them to different
  // texture units.
  glGenTextures(kYUVPlanes, textures_);

  glBindTexture(GL_TEXTURE_2D, textures_[kYPlane]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glBindTexture(GL_TEXTURE_2D, textures_[kUPlane]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  glBindTexture(GL_TEXTURE_2D, textures_[kVPlane]);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // Create our YUV->RGB shader.
  program_ = glCreateProgram();
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  const char* vs_source = kVertexShader;
  int vs_size = sizeof(kVertexShader);
  glShaderSource(vertex_shader, 1, &vs_source, &vs_size);
  glCompileShader(vertex_shader);
  int result = GL_FALSE;
  glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[kErrorSize];
    int len;
    glGetShaderInfoLog(vertex_shader, kErrorSize - 1, &len, log);
    log[kErrorSize - 1] = 0;
    LOG(FATAL) << log;
  }
  glAttachShader(program_, vertex_shader);
  glDeleteShader(vertex_shader);

  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  const char* ps_source = kFragmentShader;
  int ps_size = sizeof(kFragmentShader);
  glShaderSource(fragment_shader, 1, &ps_source, &ps_size);
  glCompileShader(fragment_shader);
  result = GL_FALSE;
  glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    char log[kErrorSize];
    int len;
    glGetShaderInfoLog(fragment_shader, kErrorSize - 1, &len, log);
    log[kErrorSize - 1] = 0;
    LOG(FATAL) << log;
  }
  glAttachShader(program_, fragment_shader);
  glDeleteShader(fragment_shader);

  glLinkProgram(program_);
  result = GL_FALSE;
  glGetProgramiv(program_, GL_LINK_STATUS, &result);
  if (!result) {
    char log[kErrorSize];
    int len;
    glGetProgramInfoLog(program_, kErrorSize - 1, &len, log);
    log[kErrorSize - 1] = 0;
    LOG(FATAL) << log;
  }
}

GpuVideoLayerGLX::~GpuVideoLayerGLX() {
  // TODO(scherkus): this seems like a bad idea.. we might be better off with
  // separate Initialize()/Teardown() calls instead.
  view_->BindContext();
  if (program_) {
    glDeleteProgram(program_);
  }

  gpu_thread_->RemoveRoute(routing_id_);
}

void GpuVideoLayerGLX::Render(const gfx::Size& viewport_size) {
  // Nothing to do if we're not visible or have no YUV data.
  if (target_rect_.IsEmpty()) {
    return;
  }

  // Calculate the position of our quad.
  CalculateVertices(viewport_size, target_rect_, target_vertices_);

  // Bind Y, U and V textures to texture units.
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textures_[kYPlane]);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, textures_[kUPlane]);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, textures_[kVPlane]);

  // Bind vertex/fragment shader program.
  glUseProgram(program_);

  // Bind parameters.
  glUniform1i(glGetUniformLocation(program_, "y_tex"), 0);
  glUniform1i(glGetUniformLocation(program_, "u_tex"), 1);
  glUniform1i(glGetUniformLocation(program_, "v_tex"), 2);

#if !defined(I915_WORKAROUND)
  int yuv2rgb_location = glGetUniformLocation(program_, "yuv2rgb");
  glUniformMatrix3fv(yuv2rgb_location, 1, GL_TRUE, kYUV2RGB);
#endif

  // TODO(scherkus): instead of calculating and loading a geometry each time,
  // we should store a constant geometry in a VBO and use a vertex shader.
  int pos_location = glGetAttribLocation(program_, "in_pos");
  glEnableVertexAttribArray(pos_location);
  glVertexAttribPointer(pos_location, 2, GL_FLOAT, GL_FALSE, 0,
                        target_vertices_);

  int tc_location = glGetAttribLocation(program_, "in_tc");
  glEnableVertexAttribArray(tc_location);
  glVertexAttribPointer(tc_location, 2, GL_FLOAT, GL_FALSE, 0,
                        kTextureCoords);

  // Render!
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // Reset back to original state.
  glDisableVertexAttribArray(pos_location);
  glDisableVertexAttribArray(tc_location);
  glActiveTexture(GL_TEXTURE0);
  glUseProgram(0);
}

void GpuVideoLayerGLX::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(GpuVideoLayerGLX, msg)
    IPC_MESSAGE_HANDLER(GpuMsg_PaintToVideoLayer, OnPaintToVideoLayer)
  IPC_END_MESSAGE_MAP_EX()
}

void GpuVideoLayerGLX::OnChannelConnected(int32 peer_pid) {
}

void GpuVideoLayerGLX::OnChannelError() {
  // FIXME(brettw) does this mean we aren't getting any more messages and we
  // should delete outselves?
  NOTIMPLEMENTED();
}

void GpuVideoLayerGLX::OnPaintToVideoLayer(TransportDIB::Handle dib_handle,
                                           const gfx::Rect& bitmap_rect) {
  TransportDIB::ScopedHandle scoped_dib_handle(dib_handle);
  // TODO(scherkus): |native_size_| is set in constructor, so perhaps this check
  // should be a DCHECK().
  const int width = native_size_.width();
  const int height = native_size_.height();
  const int stride = width;

  if (width <= 0 || width > kMaxVideoLayerSize ||
      height <= 0 || height > kMaxVideoLayerSize)
    return;

  TransportDIB* dib = TransportDIB::Map(scoped_dib_handle.release());
  if (!dib)
    return;

  // Everything looks good, update our target position and size.
  target_rect_ = bitmap_rect;

  // Perform colour space conversion.
  uint8* planes[kYUVPlanes];
  planes[kYPlane] = reinterpret_cast<uint8*>(dib->memory());
  planes[kUPlane] = planes[kYPlane] + width * height;
  planes[kVPlane] = planes[kUPlane] + ((width * height) >> 2);

  view_->BindContext();  // Must do this before issuing OpenGl.

  // Assume YV12 format.
  for (int i = 0; i < kYUVPlanes; ++i) {
    int plane_width = (i == kYPlane ? width : width / 2);
    int plane_height = (i == kYPlane ? height : height / 2);
    int plane_stride = (i == kYPlane ? stride : stride / 2);

    // Ensure that we will not read outside the shared mem region.
    if (planes[i] >= planes[kYPlane] &&
        (dib->size() - (planes[kYPlane] - planes[i])) >=
        static_cast<unsigned int>(plane_width * plane_height)) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, textures_[i]);
      glPixelStorei(GL_UNPACK_ROW_LENGTH, plane_stride);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, plane_width, plane_height, 0,
                   GL_LUMINANCE, GL_UNSIGNED_BYTE, planes[i]);
    }
  }

  // Reset back to original state.
  glActiveTexture(GL_TEXTURE0);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glFlush();

  // TODO(scherkus): we may not need to ACK video layer updates at all.
  gpu_thread_->Send(new GpuHostMsg_PaintToVideoLayer_ACK(routing_id_));
}

// static
void GpuVideoLayerGLX::CalculateVertices(const gfx::Size& world,
                                         const gfx::Rect& object,
                                         float* vertices) {
  // Don't forget GL has a flipped Y-axis!
  float width = world.width();
  float height = world.height();

  // Top left.
  vertices[0] = 2.0f * (object.x() / width) - 1.0f;
  vertices[1] = -2.0f * (object.y() / height) + 1.0f;

  // Bottom left.
  vertices[2] = 2.0f * (object.x() / width) - 1.0f;
  vertices[3] = -2.0f * (object.bottom() / height) + 1.0f;

  // Top right.
  vertices[4] = 2.0f * (object.right() / width) - 1.0f;
  vertices[5] = -2.0f * (object.y() / height) + 1.0f;

  // Bottom right.
  vertices[6] = 2.0f * (object.right() / width) - 1.0f;
  vertices[7] = -2.0f * (object.bottom() / height) + 1.0f;
}
