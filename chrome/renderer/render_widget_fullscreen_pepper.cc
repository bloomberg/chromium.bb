// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_widget_fullscreen_pepper.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/ggl/ggl.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/pepper_platform_context_3d_impl.h"
#include "chrome/renderer/render_thread.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using WebKit::WebCanvas;
using WebKit::WebCompositionUnderline;
using WebKit::WebCursorInfo;
using WebKit::WebInputEvent;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebTextDirection;
using WebKit::WebTextInputType;
using WebKit::WebVector;
using WebKit::WebWidget;

namespace {

// WebWidget that simply wraps the pepper plugin.
class PepperWidget : public WebWidget {
 public:
  PepperWidget(webkit::ppapi::PluginInstance* plugin,
               RenderWidgetFullscreenPepper* widget)
      : plugin_(plugin),
        widget_(widget),
        cursor_(WebCursorInfo::TypePointer) {
  }

  // WebWidget API
  virtual void close() {
    delete this;
  }

  virtual WebSize size() {
    return size_;
  }

  virtual void resize(const WebSize& size) {
    size_ = size;
    WebRect plugin_rect(0, 0, size_.width, size_.height);
    plugin_->ViewChanged(plugin_rect, plugin_rect);
    widget_->Invalidate();
  }

  virtual void animate() {
  }

  virtual void layout() {
  }

  virtual void paint(WebCanvas* canvas, const WebRect& rect) {
    if (!plugin_)
      return;
    WebRect plugin_rect(0, 0, size_.width, size_.height);
    plugin_->Paint(canvas, plugin_rect, rect);
  }

  virtual void composite(bool finish) {
    ggl::Context* context = widget_->context();
    DCHECK(context);
    gpu::gles2::GLES2Implementation* gl = ggl::GetImplementation(context);
    unsigned int texture = plugin_->GetBackingTextureId();
    gl->BindTexture(GL_TEXTURE_2D, texture);
    gl->DrawArrays(GL_TRIANGLES, 0, 3);
    ggl::SwapBuffers(context);
  }

  virtual void themeChanged() {
    NOTIMPLEMENTED();
  }

  virtual bool handleInputEvent(const WebInputEvent& event) {
    if (!plugin_)
      return false;
    return plugin_->HandleInputEvent(event, &cursor_);
  }

  virtual void mouseCaptureLost() {
    NOTIMPLEMENTED();
  }

  virtual void setFocus(bool focus) {
    NOTIMPLEMENTED();
  }

  virtual bool setComposition(
      const WebString& text,
      const WebVector<WebCompositionUnderline>& underlines,
      int selectionStart,
      int selectionEnd) {
    NOTIMPLEMENTED();
    return false;
  }

  virtual bool confirmComposition() {
    NOTIMPLEMENTED();
    return false;
  }

  virtual bool confirmComposition(const WebString& text) {
    NOTIMPLEMENTED();
    return false;
  }

  virtual WebTextInputType textInputType() {
    NOTIMPLEMENTED();
    return WebKit::WebTextInputTypeNone;
  }

  virtual WebRect caretOrSelectionBounds() {
    NOTIMPLEMENTED();
    return WebRect();
  }

  virtual void setTextDirection(WebTextDirection) {
    NOTIMPLEMENTED();
  }

  virtual bool isAcceleratedCompositingActive() const {
    return widget_->context() && plugin_ &&
        (plugin_->GetBackingTextureId() != 0);
  }

 private:
  webkit::ppapi::PluginInstance* plugin_;
  RenderWidgetFullscreenPepper* widget_;
  WebSize size_;
  WebCursorInfo cursor_;

  DISALLOW_COPY_AND_ASSIGN(PepperWidget);
};

}  // anonymous namespace

// static
RenderWidgetFullscreenPepper* RenderWidgetFullscreenPepper::Create(
    int32 opener_id, RenderThreadBase* render_thread,
    webkit::ppapi::PluginInstance* plugin) {
  DCHECK_NE(MSG_ROUTING_NONE, opener_id);
  scoped_refptr<RenderWidgetFullscreenPepper> widget(
      new RenderWidgetFullscreenPepper(render_thread, plugin));
  widget->Init(opener_id);
  return widget.release();
}

RenderWidgetFullscreenPepper::RenderWidgetFullscreenPepper(
    RenderThreadBase* render_thread,
    webkit::ppapi::PluginInstance* plugin)
    : RenderWidgetFullscreen(render_thread),
      plugin_(plugin),
      context_(NULL),
      buffer_(0),
      program_(0) {
}

RenderWidgetFullscreenPepper::~RenderWidgetFullscreenPepper() {
  DestroyContext();
}

void RenderWidgetFullscreenPepper::Invalidate() {
  InvalidateRect(gfx::Rect(size_.width(), size_.height()));
}

void RenderWidgetFullscreenPepper::InvalidateRect(const WebKit::WebRect& rect) {
  if (CheckCompositing()) {
    scheduleComposite();
  } else {
    didInvalidateRect(rect);
  }
}

void RenderWidgetFullscreenPepper::ScrollRect(
    int dx, int dy, const WebKit::WebRect& rect) {
  if (CheckCompositing()) {
    scheduleComposite();
  } else {
    didScrollRect(dx, dy, rect);
  }
}

void RenderWidgetFullscreenPepper::Destroy() {
  // This function is called by the plugin instance as it's going away, so reset
  // plugin_ to NULL to avoid calling into a dangling pointer e.g. on Close().
  plugin_ = NULL;
  Send(new ViewHostMsg_Close(routing_id_));
}

webkit::ppapi::PluginDelegate::PlatformContext3D*
RenderWidgetFullscreenPepper::CreateContext3D() {
  if (!context_) {
    CreateContext();
  }
  if (!context_)
    return NULL;
  return new PlatformContext3DImpl(context_);
}

void RenderWidgetFullscreenPepper::DidInitiatePaint() {
  if (plugin_)
    plugin_->ViewInitiatedPaint();
}

void RenderWidgetFullscreenPepper::DidFlushPaint() {
  if (plugin_)
    plugin_->ViewFlushedPaint();
}

void RenderWidgetFullscreenPepper::Close() {
  // If the fullscreen window is closed (e.g. user pressed escape), reset to
  // normal mode.
  if (plugin_)
    plugin_->SetFullscreen(false);
}

webkit::ppapi::PluginInstance*
RenderWidgetFullscreenPepper::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip) {
  if (plugin_ &&
      plugin_->GetBitmapForOptimizedPluginPaint(paint_bounds, dib,
                                                location, clip))
    return plugin_;
  return NULL;
}

void RenderWidgetFullscreenPepper::OnResize(const gfx::Size& size,
                                            const gfx::Rect& resizer_rect) {
  if (context_) {
    gpu::gles2::GLES2Implementation* gl = ggl::GetImplementation(context_);
#if defined(OS_MACOSX)
    ggl::ResizeOnscreenContext(context_, size);
#else
    gl->ResizeCHROMIUM(size.width(), size.height());
#endif
    gl->Viewport(0, 0, size.width(), size.height());
  }
  RenderWidget::OnResize(size, resizer_rect);
}

WebWidget* RenderWidgetFullscreenPepper::CreateWebWidget() {
  return new PepperWidget(plugin_, this);
}

void RenderWidgetFullscreenPepper::CreateContext() {
  DCHECK(!context_);
  RenderThread* render_thread = RenderThread::current();
  DCHECK(render_thread);
  GpuChannelHost* host = render_thread->EstablishGpuChannelSync();
  if (!host)
    return;
  const int32 attribs[] = {
    ggl::GGL_ALPHA_SIZE, 8,
    ggl::GGL_DEPTH_SIZE, 0,
    ggl::GGL_STENCIL_SIZE, 0,
    ggl::GGL_SAMPLES, 0,
    ggl::GGL_SAMPLE_BUFFERS, 0,
    ggl::GGL_NONE,
  };
  context_ = ggl::CreateViewContext(
      host,
      routing_id(),
      "GL_OES_packed_depth_stencil GL_OES_depth24",
      attribs);
  if (!context_ || !InitContext()) {
    DestroyContext();
    return;
  }
  ggl::SetSwapBuffersCallback(
      context_,
      NewCallback(this, &RenderWidgetFullscreenPepper::DidFlushPaint));
}

void RenderWidgetFullscreenPepper::DestroyContext() {
  if (context_) {
    gpu::gles2::GLES2Implementation* gl = ggl::GetImplementation(context_);
    if (program_) {
      gl->DeleteProgram(program_);
      program_ = 0;
    }
    if (buffer_) {
      gl->DeleteBuffers(1, &buffer_);
      buffer_ = 0;
    }
    ggl::DestroyContext(context_);
    context_ = NULL;
  }
}

namespace {

const char kVertexShader[] =
    "attribute vec2 in_tex_coord;\n"
    "varying vec2 tex_coord;\n"
    "void main() {\n"
    "  gl_Position = vec4(in_tex_coord.x * 2. - 1.,\n"
    "                     in_tex_coord.y * 2. - 1.,\n"
    "                     0.,\n"
    "                     1.);\n"
    "  tex_coord = vec2(in_tex_coord.x, in_tex_coord.y);\n"
    "}\n";

const char kFragmentShader[] =
    "precision mediump float;\n"
    "varying vec2 tex_coord;\n"
    "uniform sampler2D in_texture;\n"
    "void main() {\n"
    "  gl_FragColor = texture2D(in_texture, tex_coord);\n"
    "}\n";

GLuint CreateShaderFromSource(gpu::gles2::GLES2Implementation* gl,
                              GLenum type,
                              const char* source) {
    GLuint shader = gl->CreateShader(type);
    gl->ShaderSource(shader, 1, &source, NULL);
    gl->CompileShader(shader);
    int status;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        int size = 0;
        gl->GetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
        scoped_array<char> log(new char[size]);
        gl->GetShaderInfoLog(shader, size, NULL, log.get());
        DLOG(ERROR) << "Compilation failed: " << log.get();
        gl->DeleteShader(shader);
        shader = 0;
    }
    return shader;
}

const float kTexCoords[] = {
    0.f, 0.f,
    0.f, 2.f,
    2.f, 0.f,
};

}  // anonymous namespace

bool RenderWidgetFullscreenPepper::InitContext() {
  gpu::gles2::GLES2Implementation* gl = ggl::GetImplementation(context_);
  program_ = gl->CreateProgram();

  GLuint vertex_shader =
      CreateShaderFromSource(gl, GL_VERTEX_SHADER, kVertexShader);
  if (!vertex_shader)
    return false;
  gl->AttachShader(program_, vertex_shader);
  gl->DeleteShader(vertex_shader);

  GLuint fragment_shader =
      CreateShaderFromSource(gl, GL_FRAGMENT_SHADER, kFragmentShader);
  if (!fragment_shader)
    return false;
  gl->AttachShader(program_, fragment_shader);
  gl->DeleteShader(fragment_shader);

  gl->BindAttribLocation(program_, 0, "in_tex_coord");
  gl->LinkProgram(program_);
  int status;
  gl->GetProgramiv(program_, GL_LINK_STATUS, &status);
  if (!status) {
    int size = 0;
    gl->GetProgramiv(program_, GL_INFO_LOG_LENGTH, &size);
    scoped_array<char> log(new char[size]);
    gl->GetProgramInfoLog(program_, size, NULL, log.get());
    DLOG(ERROR) << "Link failed: " << log.get();
    return false;
  }
  gl->UseProgram(program_);
  int texture_location = gl->GetUniformLocation(program_, "in_texture");
  gl->Uniform1i(texture_location, 0);

  gl->GenBuffers(1, &buffer_);
  gl->BindBuffer(GL_ARRAY_BUFFER, buffer_);
  gl->BufferData(GL_ARRAY_BUFFER,
                 sizeof(kTexCoords),
                 kTexCoords,
                 GL_STATIC_DRAW);
  gl->VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
  gl->EnableVertexAttribArray(0);
  return true;
}

bool RenderWidgetFullscreenPepper::CheckCompositing() {
  bool compositing = webwidget_->isAcceleratedCompositingActive();
  if (compositing != is_accelerated_compositing_active_) {
    didActivateAcceleratedCompositing(compositing);
  }
  return compositing;
}
