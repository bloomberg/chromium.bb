// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/render_widget_fullscreen_pepper.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"
#include "third_party/WebKit/WebKit/chromium/public/WebWidget.h"
#include "webkit/glue/plugins/pepper_fullscreen_container.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"

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
  PepperWidget(pepper::PluginInstance* plugin,
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
    // TODO(piman): transparently scale the plugin instead of resizing it.
    plugin_->ViewChanged(plugin_rect, plugin_rect);
    widget_->GenerateFullRepaint();
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
    NOTIMPLEMENTED();
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
    // TODO(piman): see if supporting accelerated compositing makes sense.
    return false;
  }

 private:
  pepper::PluginInstance* plugin_;
  RenderWidgetFullscreenPepper* widget_;
  WebSize size_;
  WebCursorInfo cursor_;

  DISALLOW_COPY_AND_ASSIGN(PepperWidget);
};


// A FullscreenContainer that forwards the API calls to the
// RenderWidgetFullscreenPepper.
class WidgetFullscreenContainer : public pepper::FullscreenContainer {
 public:
  explicit WidgetFullscreenContainer(RenderWidgetFullscreenPepper* widget)
      : widget_(widget) {
  }
  virtual ~WidgetFullscreenContainer() { }

  virtual void Invalidate() {
    widget_->GenerateFullRepaint();
  }

  virtual void InvalidateRect(const WebKit::WebRect& rect) {
    widget_->didInvalidateRect(rect);
  }

  virtual void Destroy() {
    widget_->SendClose();
  }

 private:
  RenderWidgetFullscreenPepper* widget_;

  DISALLOW_COPY_AND_ASSIGN(WidgetFullscreenContainer);
};

}  // anonymous namespace

// static
RenderWidgetFullscreenPepper* RenderWidgetFullscreenPepper::Create(
    int32 opener_id, RenderThreadBase* render_thread,
    pepper::PluginInstance* plugin) {
  DCHECK_NE(MSG_ROUTING_NONE, opener_id);
  scoped_refptr<RenderWidgetFullscreenPepper> widget =
      new RenderWidgetFullscreenPepper(render_thread, plugin);
  widget->Init(opener_id);
  return widget.release();
}

RenderWidgetFullscreenPepper::RenderWidgetFullscreenPepper(
    RenderThreadBase* render_thread, pepper::PluginInstance* plugin)
    : RenderWidgetFullscreen(render_thread, WebKit::WebPopupTypeSelect),
      plugin_(plugin),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          container_(new WidgetFullscreenContainer(this))) {
}

RenderWidgetFullscreenPepper::~RenderWidgetFullscreenPepper() {
}

WebWidget* RenderWidgetFullscreenPepper::CreateWebWidget() {
  return new PepperWidget(plugin_, this);
}

void RenderWidgetFullscreenPepper::Close() {
  // If the fullscreen window is closed (e.g. user pressed escape), reset to
  // normal mode.
  if (plugin_)
    plugin_->SetFullscreen(false);
}

void RenderWidgetFullscreenPepper::DidInitiatePaint() {
  if (plugin_)
    plugin_->ViewInitiatedPaint();
}

void RenderWidgetFullscreenPepper::DidFlushPaint() {
  if (plugin_)
    plugin_->ViewFlushedPaint();
}

void RenderWidgetFullscreenPepper::SendClose() {
  // This function is called by the plugin instance as it's going away, so reset
  // plugin_ to NULL to avoid calling into a dangling pointer e.g. on Close().
  plugin_ = NULL;
  Send(new ViewHostMsg_Close(routing_id_));
}

void RenderWidgetFullscreenPepper::GenerateFullRepaint() {
  didInvalidateRect(gfx::Rect(size_.width(), size_.height()));
}
