// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_software_output_device_gl_adapter.h"

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"

#include <GLES2/gl2.h>

using WebKit::WebImage;
using WebKit::WebGraphicsContext3D;
using WebKit::WebSize;

namespace content {

//------------------------------------------------------------------------------

CompositorSoftwareOutputDeviceGLAdapter::
    CompositorSoftwareOutputDeviceGLAdapter(WebGraphicsContext3D* context3D)
    : initialized_(false),
      program_(0),
      vertex_shader_(0),
      fragment_shader_(0),
      vertex_buffer_(0),
      framebuffer_texture_id_(0),
      context3d_(context3D),
      locked_for_write_(false) {
}

CompositorSoftwareOutputDeviceGLAdapter::
    ~CompositorSoftwareOutputDeviceGLAdapter() {
  Destroy();
}

WebImage* CompositorSoftwareOutputDeviceGLAdapter::Lock(bool forWrite) {
  locked_for_write_ = forWrite;
  image_ = device_->accessBitmap(forWrite);
  return &image_;
}

void CompositorSoftwareOutputDeviceGLAdapter::Unlock() {
  if (locked_for_write_)
    Draw(device_->accessBitmap(false).pixelRef()->pixels());
  image_.reset();
}

void CompositorSoftwareOutputDeviceGLAdapter::DidChangeViewportSize(
    gfx::Size size) {
  if (!initialized_)
    Initialize();

  if (framebuffer_texture_size_ != gfx::Size(size))
    Resize(size);
}

void CompositorSoftwareOutputDeviceGLAdapter::Initialize() {
  // Vertex shader that flips the y axis.
  static const char g_vertex_shader[] =
    "attribute vec4 a_Position;"
    "attribute vec2 a_texCoord;"
    "varying vec2 v_texCoord;"
    "void main() {"
    "  gl_Position = a_Position;"
    "  gl_Position.y = -gl_Position.y;"
    "  v_texCoord = a_texCoord;"
    "}";

  // Pixel shader that swizzles RGBA -> BGRA.
  static const char g_fragment_shader[] =
    "precision mediump float;"
    "varying vec2 v_texCoord;"
    "uniform sampler2D s_texture;"
    "void main() {"
    "  gl_FragColor = texture2D(s_texture, v_texCoord).bgra;"
    "}";

  const GLfloat attribs[] = {
    -1.0f, -1.0f,
    1.0f, -1.0f,
    -1.0f, 1.0f,
    1.0f, 1.0f,
    0.0f, 0.0f,
    1.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 1.0f
  };

  context3d_->makeContextCurrent();

  vertex_shader_ = context3d_->createShader(GL_VERTEX_SHADER);
  context3d_->shaderSource(vertex_shader_, g_vertex_shader);
  context3d_->compileShader(vertex_shader_);

  fragment_shader_ = context3d_->createShader(GL_FRAGMENT_SHADER);
  context3d_->shaderSource(fragment_shader_, g_fragment_shader);
  context3d_->compileShader(fragment_shader_);

  program_ = context3d_->createProgram();
  context3d_->attachShader(program_, vertex_shader_);
  context3d_->attachShader(program_, fragment_shader_);

  vertex_buffer_ = context3d_->createBuffer();
  context3d_->bindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  context3d_->bufferData(GL_ARRAY_BUFFER, 16 * sizeof(float), attribs,
                         GL_STATIC_DRAW);
  context3d_->enableVertexAttribArray(0);
  context3d_->vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
  context3d_->bindAttribLocation(program_, 0, "a_Position");
  context3d_->enableVertexAttribArray(1);
  context3d_->vertexAttribPointer(
      1, 2, GL_FLOAT, GL_FALSE, 0, 8 * sizeof(GLfloat));
  context3d_->bindAttribLocation(program_, 1, "a_texCoord");

  context3d_->linkProgram(program_);
  context3d_->useProgram(program_);

  int texture_uniform = context3d_->getUniformLocation(program_, "s_texture");
  context3d_->uniform1i(texture_uniform, 0);
  context3d_->disable(GL_SCISSOR_TEST);
  context3d_->clearColor(0, 0, 1, 1);

  initialized_ = true;
}

void CompositorSoftwareOutputDeviceGLAdapter::Destroy() {
  if (!initialized_)
    return;

  context3d_->makeContextCurrent();
  context3d_->deleteShader(vertex_shader_);
  context3d_->deleteShader(fragment_shader_);
  context3d_->deleteProgram(program_);
  context3d_->deleteBuffer(vertex_buffer_);
  if (framebuffer_texture_id_)
    context3d_->deleteTexture(framebuffer_texture_id_);
}

void CompositorSoftwareOutputDeviceGLAdapter::Resize(
    const gfx::Size& viewport_size) {
  framebuffer_texture_size_ = viewport_size;
  device_.reset(new SkDevice(SkBitmap::kARGB_8888_Config,
      viewport_size.width(), viewport_size.height(), true));

  context3d_->makeContextCurrent();
  context3d_->ensureBackbufferCHROMIUM();
  framebuffer_texture_id_ = context3d_->createTexture();
  context3d_->bindTexture(GL_TEXTURE_2D, framebuffer_texture_id_);
  context3d_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  context3d_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  context3d_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  context3d_->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  context3d_->viewport(0, 0, viewport_size.width(), viewport_size.height());
  context3d_->reshape(viewport_size.width(), viewport_size.height());
}

void CompositorSoftwareOutputDeviceGLAdapter::Draw(void* pixels) {
  TRACE_EVENT0("renderer", "CompositorSoftwareOutputDeviceGLAdapter::Draw");
  if (!initialized_)
    NOTREACHED();
  if (!framebuffer_texture_id_)
    NOTREACHED();

  context3d_->makeContextCurrent();
  context3d_->ensureBackbufferCHROMIUM();
  context3d_->clear(GL_COLOR_BUFFER_BIT);

  context3d_->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
      framebuffer_texture_size_.width(), framebuffer_texture_size_.height(),
      0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

  context3d_->drawArrays(GL_TRIANGLE_STRIP, 0, 4);

  context3d_->prepareTexture();
}

}  // namespace content
