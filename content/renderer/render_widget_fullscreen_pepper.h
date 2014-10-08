// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
#define CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_

#include "base/memory/scoped_ptr.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/pepper/fullscreen_container.h"
#include "content/renderer/render_widget_fullscreen.h"
#include "third_party/WebKit/public/web/WebWidget.h"

namespace blink {
class WebLayer;
}

namespace content {
class PepperPluginInstanceImpl;

// A RenderWidget that hosts a fullscreen pepper plugin. This provides a
// FullscreenContainer that the plugin instance can callback into to e.g.
// invalidate rects.
class RenderWidgetFullscreenPepper : public RenderWidgetFullscreen,
                                     public FullscreenContainer {
 public:
  static RenderWidgetFullscreenPepper* Create(
      int32 opener_id,
      PepperPluginInstanceImpl* plugin,
      const GURL& active_url,
      const blink::WebScreenInfo& screen_info);

  // pepper::FullscreenContainer API.
  virtual void Invalidate() override;
  virtual void InvalidateRect(const blink::WebRect& rect) override;
  virtual void ScrollRect(int dx, int dy, const blink::WebRect& rect) override;
  virtual void Destroy() override;
  virtual void DidChangeCursor(const blink::WebCursorInfo& cursor) override;
  virtual void SetLayer(blink::WebLayer* layer) override;

  // IPC::Listener implementation. This overrides the implementation
  // in RenderWidgetFullscreen.
  virtual bool OnMessageReceived(const IPC::Message& msg) override;

  // Could be NULL when this widget is closing.
  PepperPluginInstanceImpl* plugin() const { return plugin_; }

  MouseLockDispatcher* mouse_lock_dispatcher() const {
    return mouse_lock_dispatcher_.get();
  }

 protected:
  RenderWidgetFullscreenPepper(PepperPluginInstanceImpl* plugin,
                               const GURL& active_url,
                               const blink::WebScreenInfo& screen_info);
  virtual ~RenderWidgetFullscreenPepper();

  // RenderWidget API.
  virtual void DidInitiatePaint() override;
  virtual void DidFlushPaint() override;
  virtual void Close() override;
  virtual void OnResize(const ViewMsg_Resize_Params& params) override;

  // RenderWidgetFullscreen API.
  virtual blink::WebWidget* CreateWebWidget() override;

  // RenderWidget overrides.
  virtual GURL GetURLForGraphicsContext3D() override;
  virtual void SetDeviceScaleFactor(float device_scale_factor) override;

 private:
  // URL that is responsible for this widget, passed to ggl::CreateViewContext.
  GURL active_url_;

  // The plugin instance this widget wraps.
  PepperPluginInstanceImpl* plugin_;

  blink::WebLayer* layer_;

  scoped_ptr<MouseLockDispatcher> mouse_lock_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetFullscreenPepper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
