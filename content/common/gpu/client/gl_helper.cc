// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gl_helper.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "ui/gfx/gl/gl_bindings.h"
#include "ui/gfx/size.h"

namespace {

class ScopedWebGLId {
 public:
  typedef void (WebKit::WebGraphicsContext3D::*DeleteFunc)(WebKit::WebGLId);
  ScopedWebGLId(WebKit::WebGraphicsContext3D* context,
                WebKit::WebGLId id,
                DeleteFunc delete_func)
      : context_(context),
        id_(id),
        delete_func_(delete_func) {
  }

  operator WebKit::WebGLId() const {
    return id_;
  }

  WebKit::WebGLId Detach() {
    WebKit::WebGLId id = id_;
    id_ = 0;
    return id;
  }

  virtual ~ScopedWebGLId() {
    if (id_ != 0)
      (context_->*delete_func_)(id_);
  }

 private:
  WebKit::WebGraphicsContext3D* context_;
  WebKit::WebGLId id_;
  DeleteFunc delete_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWebGLId);
};

class ScopedBuffer : public ScopedWebGLId {
 public:
  ScopedBuffer(WebKit::WebGraphicsContext3D* context,
               WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteBuffer) {}
};

class ScopedFramebuffer : public ScopedWebGLId {
 public:
  ScopedFramebuffer(WebKit::WebGraphicsContext3D* context,
                    WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteFramebuffer) {}
};

class ScopedRenderbuffer : public ScopedWebGLId {
 public:
  ScopedRenderbuffer(WebKit::WebGraphicsContext3D* context,
                     WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteRenderbuffer) {}
};

class ScopedProgram : public ScopedWebGLId {
 public:
  ScopedProgram(WebKit::WebGraphicsContext3D* context,
                WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteProgram) {}
};

class ScopedShader : public ScopedWebGLId {
 public:
  ScopedShader(WebKit::WebGraphicsContext3D* context,
               WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteShader) {}
};

class ScopedTexture : public ScopedWebGLId {
 public:
  ScopedTexture(WebKit::WebGraphicsContext3D* context,
                WebKit::WebGLId id)
      : ScopedWebGLId(context,
                      id,
                      &WebKit::WebGraphicsContext3D::deleteTexture) {}
};

template <WebKit::WGC3Denum target>
class ScopedBinder {
 public:
  typedef void (WebKit::WebGraphicsContext3D::*BindFunc)(WebKit::WGC3Denum,
                                                         WebKit::WebGLId);
  ScopedBinder(WebKit::WebGraphicsContext3D* context,
               WebKit::WebGLId id,
               BindFunc bind_func)
      : context_(context),
        id_(id),
        bind_func_(bind_func) {
    (context_->*bind_func_)(target, id_);
  }

  virtual ~ScopedBinder() {
    if (id_ != 0)
      (context_->*bind_func_)(target, 0);
  }

 private:
  WebKit::WebGraphicsContext3D* context_;
  WebKit::WebGLId id_;
  BindFunc bind_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedBinder);
};

template <WebKit::WGC3Denum target>
class ScopedBufferBinder : ScopedBinder<target> {
 public:
  ScopedBufferBinder(WebKit::WebGraphicsContext3D* context,
                     WebKit::WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebKit::WebGraphicsContext3D::bindBuffer) {}
};

template <WebKit::WGC3Denum target>
class ScopedFramebufferBinder : ScopedBinder<target> {
 public:
  ScopedFramebufferBinder(WebKit::WebGraphicsContext3D* context,
                          WebKit::WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebKit::WebGraphicsContext3D::bindFramebuffer) {}
};

template <WebKit::WGC3Denum target>
class ScopedRenderbufferBinder : ScopedBinder<target> {
 public:
  ScopedRenderbufferBinder(WebKit::WebGraphicsContext3D* context,
                           WebKit::WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebKit::WebGraphicsContext3D::bindRenderbuffer) {}
};

template <WebKit::WGC3Denum target>
class ScopedTextureBinder : ScopedBinder<target> {
 public:
  ScopedTextureBinder(WebKit::WebGraphicsContext3D* context,
                      WebKit::WebGLId id)
      : ScopedBinder<target>(
          context,
          id,
          &WebKit::WebGraphicsContext3D::bindTexture) {}
};

}  // namespace

namespace content {

// Implements GLHelper::CopyTextureTo and encapsulates the data needed for it.
class GLHelper::CopyTextureToImpl {
 public:
  CopyTextureToImpl(WebKit::WebGraphicsContext3D* context,
                    GLHelper* helper)
      : context_(context),
        helper_(helper),
        program_(context, context->createProgram()),
        vertex_attributes_buffer_(context_, context_->createBuffer()) {
    InitBuffer();
    InitProgram();
  }
  ~CopyTextureToImpl() {}

  void InitBuffer();
  void InitProgram();
  void Detach();

  bool CopyTextureTo(WebKit::WebGLId src_texture,
                     const gfx::Size& src_size,
                     const gfx::Size& dst_size,
                     unsigned char* out);
 private:
  // Interleaved array of 2-dimentional vertex positions (x, y) and
  // 2-dimentional texture coordinates (s, t).
  static const WebKit::WGC3Dfloat kVertexAttributes[];
  // Shader sources used for GLHelper::CopyTextureTo
  static const WebKit::WGC3Dchar kCopyVertexShader[];
  static const WebKit::WGC3Dchar kCopyFragmentShader[];

  WebKit::WebGraphicsContext3D* context_;
  GLHelper* helper_;

  // A program for copying a source texture into a destination texture.
  ScopedProgram program_;
  // The buffer that holds the vertices and the texture coordinates data for
  // drawing a quad.
  ScopedBuffer vertex_attributes_buffer_;
  // The location of the position in the program.
  WebKit::WGC3Dint position_location_;
  // The location of the texture coordinate in the program.
  WebKit::WGC3Dint texcoord_location_;
  // The location of the source texture in the program.
  WebKit::WGC3Dint texture_location_;
};

const WebKit::WGC3Dfloat GLHelper::CopyTextureToImpl::kVertexAttributes[] = {
  -1.0f, -1.0f, 0.0f, 0.0f,
  1.0f, -1.0f, 1.0f, 0.0f,
  -1.0f, 1.0f, 0.0f, 1.0f,
  1.0f, 1.0f, 1.0f, 1.0f,
};

const WebKit::WGC3Dchar GLHelper::CopyTextureToImpl::kCopyVertexShader[] =
    "attribute vec2 a_position;"
    "attribute vec2 a_texcoord;"
    "varying vec2 v_texcoord;"
    "void main() {"
    "  gl_Position = vec4(a_position, 0.0, 1.0);"
    "  v_texcoord = a_texcoord;"
    "}";

const WebKit::WGC3Dchar GLHelper::CopyTextureToImpl::kCopyFragmentShader[] =
    "precision mediump float;"
    "varying vec2 v_texcoord;"
    "uniform sampler2D s_texture;"
    "void main() {"
    "  gl_FragColor = texture2D(s_texture, v_texcoord);"
    "}";

void GLHelper::CopyTextureToImpl::InitBuffer() {
  ScopedBufferBinder<GL_ARRAY_BUFFER> buffer_binder(
      context_, vertex_attributes_buffer_);
  context_->bufferData(GL_ARRAY_BUFFER,
                       sizeof(kVertexAttributes),
                       kVertexAttributes,
                       GL_STATIC_DRAW);
}

void GLHelper::CopyTextureToImpl::InitProgram() {
  // Shaders to map the source texture to |dst_texture_|.
  ScopedShader vertex_shader(context_, helper_->CompileShaderFromSource(
      kCopyVertexShader, GL_VERTEX_SHADER));
  DCHECK_NE(0U, static_cast<WebKit::WebGLId>(vertex_shader));
  context_->attachShader(program_, vertex_shader);
  ScopedShader fragment_shader(context_, helper_->CompileShaderFromSource(
      kCopyFragmentShader, GL_FRAGMENT_SHADER));
  DCHECK_NE(0U, static_cast<WebKit::WebGLId>(fragment_shader));
  context_->attachShader(program_, fragment_shader);
  context_->linkProgram(program_);

  WebKit::WGC3Dint link_status = 0;
  context_->getProgramiv(program_, GL_LINK_STATUS, &link_status);
  if (!link_status) {
    LOG(ERROR) << std::string(context_->getProgramInfoLog(program_).utf8());
    return;
  }

  position_location_ = context_->getAttribLocation(program_, "a_position");
  texcoord_location_ = context_->getAttribLocation(program_, "a_texcoord");
  texture_location_ = context_->getUniformLocation(program_, "s_texture");
}

void GLHelper::CopyTextureToImpl::Detach() {
  program_.Detach();
  vertex_attributes_buffer_.Detach();
}

bool GLHelper::CopyTextureToImpl::CopyTextureTo(WebKit::WebGLId src_texture,
                                                const gfx::Size& src_size,
                                                const gfx::Size& dst_size,
                                                unsigned char* out) {
  ScopedFramebuffer dst_framebuffer(context_, context_->createFramebuffer());
  ScopedTexture dst_texture(context_, context_->createTexture());
  {
    ScopedFramebufferBinder<GL_DRAW_FRAMEBUFFER> framebuffer_binder(
        context_, dst_framebuffer);
    {
      ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, dst_texture);
      context_->texImage2D(GL_TEXTURE_2D,
                           0,
                           GL_RGBA,
                           dst_size.width(),
                           dst_size.height(),
                           0,
                           GL_RGBA,
                           GL_UNSIGNED_BYTE,
                           NULL);
      context_->framebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                     GL_COLOR_ATTACHMENT0,
                                     GL_TEXTURE_2D,
                                     dst_texture,
                                     0);
    }

    ScopedTextureBinder<GL_TEXTURE_2D> texture_binder(context_, src_texture);
    ScopedBufferBinder<GL_ARRAY_BUFFER> buffer_binder(
        context_, vertex_attributes_buffer_);

    context_->viewport(0, 0, dst_size.width(), dst_size.height());
    context_->useProgram(program_);

    WebKit::WGC3Dintptr offset = 0;
    context_->vertexAttribPointer(position_location_,
                                  2,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  4 * sizeof(WebKit::WGC3Dfloat),
                                  offset);
    context_->enableVertexAttribArray(position_location_);

    offset += 2 * sizeof(WebKit::WGC3Dfloat);
    context_->vertexAttribPointer(texcoord_location_,
                                  2,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  4 * sizeof(WebKit::WGC3Dfloat),
                                  offset);
    context_->enableVertexAttribArray(texcoord_location_);

    context_->uniform1i(texture_location_, 0);

    // Conduct texture mapping by drawing a quad composed of two triangles.
    context_->drawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }

  // TODO(mazda): Do this on another thread because this can block the UI
  // thread for possibly a long time (http://crbug.com/118547).
  return context_->readBackFramebuffer(
      out,
      4 * dst_size.width() * dst_size.height(),
      dst_framebuffer,
      dst_size.width(),
      dst_size.height());
}

GLHelper::GLHelper(WebKit::WebGraphicsContext3D* context)
    : context_(context) {
}

GLHelper::~GLHelper() {
}

WebKit::WebGraphicsContext3D* GLHelper::context() const {
  return context_;
}

void GLHelper::Detach() {
  if (copy_texture_to_impl_.get())
    copy_texture_to_impl_->Detach();
}

bool GLHelper::CopyTextureTo(WebKit::WebGLId src_texture,
                             const gfx::Size& src_size,
                             const gfx::Size& dst_size,
                             unsigned char* out) {
  // Lazily initialize |copy_texture_to_impl_|
  if (!copy_texture_to_impl_.get())
    copy_texture_to_impl_.reset(new CopyTextureToImpl(context_, this));

  return copy_texture_to_impl_->CopyTextureTo(src_texture,
                                              src_size,
                                              dst_size,
                                              out);

}

WebKit::WebGLId GLHelper::CompileShaderFromSource(
    const WebKit::WGC3Dchar* source,
    WebKit::WGC3Denum type) {
  ScopedShader shader(context_, context_->createShader(type));
  context_->shaderSource(shader, source);
  context_->compileShader(shader);
  WebKit::WGC3Dint compile_status = 0;
  context_->getShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  if (!compile_status) {
    LOG(ERROR) << std::string(context_->getShaderInfoLog(shader).utf8());
    return 0;
  }
  return shader.Detach();
}

}  // namespace content
