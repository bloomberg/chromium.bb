// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget_fullscreen_pepper.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_switches.h"
#include "content/renderer/pepper/pepper_platform_context_3d_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCanvas.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gl/gpu_preference.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

using WebKit::WebCanvas;
using WebKit::WebCompositionUnderline;
using WebKit::WebCursorInfo;
using WebKit::WebGestureEvent;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebSize;
using WebKit::WebString;
using WebKit::WebTextDirection;
using WebKit::WebTextInputType;
using WebKit::WebVector;
using WebKit::WebWidget;
using WebKit::WGC3Dintptr;

namespace content {

namespace {

// See third_party/WebKit/Source/WebCore/dom/WheelEvent.h.
const float kTickDivisor = 120.0f;

class FullscreenMouseLockDispatcher : public MouseLockDispatcher {
 public:
  explicit FullscreenMouseLockDispatcher(RenderWidgetFullscreenPepper* widget);
  virtual ~FullscreenMouseLockDispatcher();

 private:
  // MouseLockDispatcher implementation.
  virtual void SendLockMouseRequest(bool unlocked_by_target) OVERRIDE;
  virtual void SendUnlockMouseRequest() OVERRIDE;

  RenderWidgetFullscreenPepper* widget_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenMouseLockDispatcher);
};

WebMouseEvent WebMouseEventFromGestureEvent(const WebGestureEvent& gesture) {
  WebMouseEvent mouse;

  switch (gesture.type) {
    case WebInputEvent::GestureScrollBegin:
      mouse.type = WebInputEvent::MouseDown;
      break;

    case WebInputEvent::GestureScrollUpdate:
      mouse.type = WebInputEvent::MouseMove;
      break;

    case WebInputEvent::GestureFlingStart:
      if (gesture.sourceDevice == WebGestureEvent::Touchscreen) {
        // A scroll gesture on the touchscreen may end with a GestureScrollEnd
        // when there is no velocity, or a GestureFlingStart when it has a
        // velocity. In both cases, it should end the drag that was initiated by
        // the GestureScrollBegin (and subsequent GestureScrollUpdate) events.
        mouse.type = WebInputEvent::MouseUp;
        break;
      } else {
        return mouse;
      }
    case WebInputEvent::GestureScrollEnd:
      mouse.type = WebInputEvent::MouseUp;
      break;

    default:
      break;
  }

  if (mouse.type == WebInputEvent::Undefined)
    return mouse;

  mouse.timeStampSeconds = gesture.timeStampSeconds;
  mouse.modifiers = gesture.modifiers | WebInputEvent::LeftButtonDown;
  mouse.button = WebMouseEvent::ButtonLeft;
  mouse.clickCount = (mouse.type == WebInputEvent::MouseDown ||
                      mouse.type == WebInputEvent::MouseUp);

  mouse.x = gesture.x;
  mouse.y = gesture.y;
  mouse.windowX = gesture.globalX;
  mouse.windowY = gesture.globalY;
  mouse.globalX = gesture.globalX;
  mouse.globalY = gesture.globalY;

  return mouse;
}

FullscreenMouseLockDispatcher::FullscreenMouseLockDispatcher(
    RenderWidgetFullscreenPepper* widget) : widget_(widget) {
}

FullscreenMouseLockDispatcher::~FullscreenMouseLockDispatcher() {
}

void FullscreenMouseLockDispatcher::SendLockMouseRequest(
    bool unlocked_by_target) {
  widget_->Send(new ViewHostMsg_LockMouse(widget_->routing_id(), false,
                                          unlocked_by_target, true));
}

void FullscreenMouseLockDispatcher::SendUnlockMouseRequest() {
  widget_->Send(new ViewHostMsg_UnlockMouse(widget_->routing_id()));
}

// WebWidget that simply wraps the pepper plugin.
class PepperWidget : public WebWidget {
 public:
  explicit PepperWidget(RenderWidgetFullscreenPepper* widget)
      : widget_(widget) {
  }

  virtual ~PepperWidget() {}

  // WebWidget API
  virtual void close() {
    delete this;
  }

  virtual WebSize size() {
    return size_;
  }

  virtual void willStartLiveResize() {
  }

  virtual void resize(const WebSize& size) {
    if (!widget_->plugin())
      return;

    size_ = size;
    WebRect plugin_rect(0, 0, size_.width, size_.height);
    widget_->plugin()->ViewChanged(plugin_rect, plugin_rect,
                                   std::vector<gfx::Rect>());
    widget_->Invalidate();
  }

  virtual void willEndLiveResize() {
  }

  virtual void animate(double frameBeginTime) {
  }

  virtual void layout() {
  }

  virtual void paint(WebCanvas* canvas, const WebRect& rect, PaintOptions) {
    if (!widget_->plugin())
      return;

    SkAutoCanvasRestore auto_restore(canvas, true);
    float canvas_scale = widget_->deviceScaleFactor();
    canvas->scale(canvas_scale, canvas_scale);

    WebRect plugin_rect(0, 0, size_.width, size_.height);
    widget_->plugin()->Paint(canvas, plugin_rect, rect);
  }

  virtual void setCompositorSurfaceReady() {
  }

  virtual void composite(bool finish) {
  }

  virtual void themeChanged() {
    NOTIMPLEMENTED();
  }

  virtual bool handleInputEvent(const WebInputEvent& event) {
    if (!widget_->plugin())
      return false;

    // This cursor info is ignored, we always set the cursor directly from
    // RenderWidgetFullscreenPepper::DidChangeCursor.
    WebCursorInfo cursor;

    // Pepper plugins do not accept gesture events. So do not send the gesture
    // events directly to the plugin. Instead, try to convert them to equivalent
    // mouse events, and then send to the plugin.
    if (WebInputEvent::isGestureEventType(event.type)) {
      bool result = false;
      const WebGestureEvent* gesture_event =
          static_cast<const WebGestureEvent*>(&event);
      switch (event.type) {
        case WebInputEvent::GestureTap: {
          WebMouseEvent mouse;

          mouse.timeStampSeconds = gesture_event->timeStampSeconds;
          mouse.type = WebInputEvent::MouseMove;
          mouse.modifiers = gesture_event->modifiers;

          mouse.x = gesture_event->x;
          mouse.y = gesture_event->y;
          mouse.windowX = gesture_event->globalX;
          mouse.windowY = gesture_event->globalY;
          mouse.globalX = gesture_event->globalX;
          mouse.globalY = gesture_event->globalY;
          mouse.movementX = 0;
          mouse.movementY = 0;
          result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);

          mouse.type = WebInputEvent::MouseDown;
          mouse.button = WebMouseEvent::ButtonLeft;
          mouse.clickCount = gesture_event->data.tap.tapCount;
          result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);

          mouse.type = WebInputEvent::MouseUp;
          result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);
          break;
        }

        default: {
          WebMouseEvent mouse = WebMouseEventFromGestureEvent(*gesture_event);
          if (mouse.type != WebInputEvent::Undefined)
            result |= widget_->plugin()->HandleInputEvent(mouse, &cursor);
          break;
        }
      }
      return result;
    }

    bool result = widget_->plugin()->HandleInputEvent(event, &cursor);

    // For normal web pages, WebViewImpl does input event translations and
    // generates context menu events. Since we don't have a WebView, we need to
    // do the necessary translation ourselves.
    if (WebInputEvent::isMouseEventType(event.type)) {
      const WebMouseEvent& mouse_event =
          reinterpret_cast<const WebMouseEvent&>(event);
      bool send_context_menu_event = false;
      // On Mac/Linux, we handle it on mouse down.
      // On Windows, we handle it on mouse up.
#if defined(OS_WIN)
      send_context_menu_event =
          mouse_event.type == WebInputEvent::MouseUp &&
          mouse_event.button == WebMouseEvent::ButtonRight;
#elif defined(OS_MACOSX)
      send_context_menu_event =
          mouse_event.type == WebInputEvent::MouseDown &&
          (mouse_event.button == WebMouseEvent::ButtonRight ||
           (mouse_event.button == WebMouseEvent::ButtonLeft &&
            mouse_event.modifiers & WebMouseEvent::ControlKey));
#else
      send_context_menu_event =
          mouse_event.type == WebInputEvent::MouseDown &&
          mouse_event.button == WebMouseEvent::ButtonRight;
#endif
      if (send_context_menu_event) {
        WebMouseEvent context_menu_event(mouse_event);
        context_menu_event.type = WebInputEvent::ContextMenu;
        widget_->plugin()->HandleInputEvent(context_menu_event, &cursor);
      }
    }
    return result;
  }

  virtual void mouseCaptureLost() {
    NOTIMPLEMENTED();
  }

  virtual void setFocus(bool focus) {
    NOTIMPLEMENTED();
  }

  // TODO(piman): figure out IME and implement these if necessary.
  virtual bool setComposition(
      const WebString& text,
      const WebVector<WebCompositionUnderline>& underlines,
      int selectionStart,
      int selectionEnd) {
    return false;
  }

  virtual bool confirmComposition() {
    return false;
  }

  virtual bool compositionRange(size_t* location, size_t* length) {
    return false;
  }

  virtual bool confirmComposition(const WebString& text) {
    return false;
  }

  virtual WebTextInputType textInputType() {
    return WebKit::WebTextInputTypeNone;
  }

  virtual WebRect caretOrSelectionBounds() {
    return WebRect();
  }

  virtual bool selectionRange(WebPoint& start, WebPoint& end) const {
    return false;
  }

  virtual bool caretOrSelectionRange(size_t* location, size_t* length) {
    return false;
  }

  virtual void setTextDirection(WebTextDirection) {
  }

  virtual bool isAcceleratedCompositingActive() const {
    return widget_->context() && widget_->plugin() &&
        (widget_->plugin()->GetBackingTextureId() != 0);
  }

 private:
  RenderWidgetFullscreenPepper* widget_;
  WebSize size_;

  DISALLOW_COPY_AND_ASSIGN(PepperWidget);
};

void DestroyContext(WebGraphicsContext3DCommandBufferImpl* context,
                    GLuint program,
                    GLuint buffer) {
  DCHECK(context);
  if (program)
    context->deleteProgram(program);
  if (buffer)
    context->deleteBuffer(buffer);
  delete context;
}

}  // anonymous namespace

// static
RenderWidgetFullscreenPepper* RenderWidgetFullscreenPepper::Create(
    int32 opener_id, webkit::ppapi::PluginInstance* plugin,
    const GURL& active_url,
    const WebKit::WebScreenInfo& screen_info) {
  DCHECK_NE(MSG_ROUTING_NONE, opener_id);
  scoped_refptr<RenderWidgetFullscreenPepper> widget(
      new RenderWidgetFullscreenPepper(plugin, active_url, screen_info));
  widget->Init(opener_id);
  widget->AddRef();
  return widget.get();
}

RenderWidgetFullscreenPepper::RenderWidgetFullscreenPepper(
    webkit::ppapi::PluginInstance* plugin,
    const GURL& active_url,
    const WebKit::WebScreenInfo& screen_info)
    : RenderWidgetFullscreen(screen_info),
      active_url_(active_url),
      plugin_(plugin),
      context_(NULL),
      buffer_(0),
      program_(0),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      mouse_lock_dispatcher_(new FullscreenMouseLockDispatcher(
          ALLOW_THIS_IN_INITIALIZER_LIST(this))) {
}

RenderWidgetFullscreenPepper::~RenderWidgetFullscreenPepper() {
  if (context_)
    DestroyContext(context_, program_, buffer_);
}

void RenderWidgetFullscreenPepper::OnViewContextSwapBuffersPosted() {
  OnSwapBuffersPosted();
}

void RenderWidgetFullscreenPepper::OnViewContextSwapBuffersComplete() {
  OnSwapBuffersComplete();
}

void RenderWidgetFullscreenPepper::OnViewContextSwapBuffersAborted() {
  if (!context_)
    return;
  // Destroy the context later, in case we got called from InitContext for
  // example. We still need to reset context_ now so that a new context gets
  // created when the plugin recreates its own.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&DestroyContext, context_, program_, buffer_));
  context_ = NULL;
  program_ = 0;
  buffer_ = 0;
  OnSwapBuffersAborted();
  CheckCompositing();
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
  Release();
}

void RenderWidgetFullscreenPepper::DidChangeCursor(
    const WebKit::WebCursorInfo& cursor) {
  didChangeCursor(cursor);
}

webkit::ppapi::PluginDelegate::PlatformContext3D*
RenderWidgetFullscreenPepper::CreateContext3D() {
#ifdef ENABLE_GPU
  return new PlatformContext3DImpl(this);
#else
  return NULL;
#endif
}

void RenderWidgetFullscreenPepper::ReparentContext(
    webkit::ppapi::PluginDelegate::PlatformContext3D* context) {
  static_cast<PlatformContext3DImpl*>(context)->SetParentContext(this);
}

bool RenderWidgetFullscreenPepper::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderWidgetFullscreenPepper, msg)
    IPC_MESSAGE_FORWARD(ViewMsg_LockMouse_ACK,
                        mouse_lock_dispatcher_.get(),
                        MouseLockDispatcher::OnLockMouseACK)
    IPC_MESSAGE_FORWARD(ViewMsg_MouseLockLost,
                        mouse_lock_dispatcher_.get(),
                        MouseLockDispatcher::OnMouseLockLost)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  if (handled)
    return true;

  return RenderWidgetFullscreen::OnMessageReceived(msg);
}

void RenderWidgetFullscreenPepper::WillInitiatePaint() {
  if (plugin_)
    plugin_->ViewWillInitiatePaint();
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
    plugin_->FlashSetFullscreen(false, false);

  // Call Close on the base class to destroy the WebWidget instance.
  RenderWidget::Close();
}

webkit::ppapi::PluginInstance*
RenderWidgetFullscreenPepper::GetBitmapForOptimizedPluginPaint(
    const gfx::Rect& paint_bounds,
    TransportDIB** dib,
    gfx::Rect* location,
    gfx::Rect* clip,
    float* scale_factor) {
  if (plugin_ && plugin_->GetBitmapForOptimizedPluginPaint(
          paint_bounds, dib, location, clip, scale_factor)) {
    return plugin_;
  }
  return NULL;
}

void RenderWidgetFullscreenPepper::OnResize(const gfx::Size& size,
                                            const gfx::Rect& resizer_rect,
                                            bool is_fullscreen) {
  if (context_) {
    gfx::Size pixel_size = gfx::ToFlooredSize(
        gfx::ScaleSize(size, deviceScaleFactor()));
    context_->reshape(pixel_size.width(), pixel_size.height());
    context_->viewport(0, 0, pixel_size.width(), pixel_size.height());
  }
  RenderWidget::OnResize(size, resizer_rect, is_fullscreen);
}

WebWidget* RenderWidgetFullscreenPepper::CreateWebWidget() {
  return new PepperWidget(this);
}

bool RenderWidgetFullscreenPepper::SupportsAsynchronousSwapBuffers() {
  return context_ != NULL;
}

// Fullscreen pepper widgets composite themselves into the plugin's backing
// texture (as opposed to using the cc library to composite as normal
// content::RenderWidgets do), so to produce a composited frame we just have to
// draw this texture and swap.
void RenderWidgetFullscreenPepper::Composite() {
  if (!plugin_)
    return;

  DCHECK(context_);
  unsigned int texture = plugin_->GetBackingTextureId();
  context_->bindTexture(GL_TEXTURE_2D, texture);
  context_->drawArrays(GL_TRIANGLES, 0, 3);
  SwapBuffers();
}

void RenderWidgetFullscreenPepper::CreateContext() {
  DCHECK(!context_);
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableFlashFullscreen3d))
    return;
  WebKit::WebGraphicsContext3D::Attributes attributes;
  attributes.depth = false;
  attributes.stencil = false;
  attributes.antialias = false;
  attributes.shareResources = false;
  attributes.preferDiscreteGPU = true;
  context_ = WebGraphicsContext3DCommandBufferImpl::CreateViewContext(
      RenderThreadImpl::current(),
      surface_id(),
      NULL,
      attributes,
      true /* bind generates resources */,
      active_url_,
      CAUSE_FOR_GPU_LAUNCH_RENDERWIDGETFULLSCREENPEPPER_CREATECONTEXT);
  if (!context_)
    return;

  if (!InitContext()) {
    DestroyContext(context_, program_, buffer_);
    context_ = NULL;
    return;
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

GLuint CreateShaderFromSource(WebGraphicsContext3DCommandBufferImpl* context,
                              GLenum type,
                              const char* source) {
    GLuint shader = context->createShader(type);
    context->shaderSource(shader, source);
    context->compileShader(shader);
    int status = GL_FALSE;
    context->getShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        int size = 0;
        context->getShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
        std::string log = context->getShaderInfoLog(shader).utf8();
        DLOG(ERROR) << "Compilation failed: " << log;
        context->deleteShader(shader);
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
  gfx::Size pixel_size = gfx::ToFlooredSize(
      gfx::ScaleSize(size(), deviceScaleFactor()));
  context_->reshape(pixel_size.width(), pixel_size.height());
  context_->viewport(0, 0, pixel_size.width(), pixel_size.height());

  program_ = context_->createProgram();

  GLuint vertex_shader =
      CreateShaderFromSource(context_, GL_VERTEX_SHADER, kVertexShader);
  if (!vertex_shader)
    return false;
  context_->attachShader(program_, vertex_shader);
  context_->deleteShader(vertex_shader);

  GLuint fragment_shader =
      CreateShaderFromSource(context_, GL_FRAGMENT_SHADER, kFragmentShader);
  if (!fragment_shader)
    return false;
  context_->attachShader(program_, fragment_shader);
  context_->deleteShader(fragment_shader);

  context_->bindAttribLocation(program_, 0, "in_tex_coord");
  context_->linkProgram(program_);
  int status = GL_FALSE;
  context_->getProgramiv(program_, GL_LINK_STATUS, &status);
  if (!status) {
    int size = 0;
    context_->getProgramiv(program_, GL_INFO_LOG_LENGTH, &size);
    std::string log = context_->getProgramInfoLog(program_).utf8();
    DLOG(ERROR) << "Link failed: " << log;
    return false;
  }
  context_->useProgram(program_);
  int texture_location = context_->getUniformLocation(program_, "in_texture");
  context_->uniform1i(texture_location, 0);

  buffer_ = context_->createBuffer();
  context_->bindBuffer(GL_ARRAY_BUFFER, buffer_);
  context_->bufferData(GL_ARRAY_BUFFER,
                 sizeof(kTexCoords),
                 kTexCoords,
                 GL_STATIC_DRAW);
  context_->vertexAttribPointer(0, 2,
                                GL_FLOAT, GL_FALSE,
                                0, static_cast<WGC3Dintptr>(NULL));
  context_->enableVertexAttribArray(0);
  return true;
}

bool RenderWidgetFullscreenPepper::CheckCompositing() {
  bool compositing =
      webwidget_ && webwidget_->isAcceleratedCompositingActive();
  if (compositing != is_accelerated_compositing_active_) {
    if (compositing)
      didActivateCompositor(-1);
    else
      didDeactivateCompositor();
  }
  return compositing;
}

void RenderWidgetFullscreenPepper::SwapBuffers() {
  DCHECK(context_);
  context_->prepareTexture();

  // The compositor isn't actually active in this path, but pretend it is for
  // scheduling purposes.
  didCommitAndDrawCompositorFrame();
}

WebGraphicsContext3DCommandBufferImpl*
RenderWidgetFullscreenPepper::GetParentContextForPlatformContext3D() {
  if (!context_) {
    CreateContext();
  }
  if (!context_)
    return NULL;
  return context_;
}

}  // namespace content
