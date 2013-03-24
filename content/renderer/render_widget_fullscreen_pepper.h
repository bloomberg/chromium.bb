// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
#define CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_

#include "base/memory/scoped_ptr.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/render_widget_fullscreen.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"
#include "webkit/plugins/ppapi/fullscreen_container.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

}  // namespace ppapi
}  // namespace webkit

namespace WebKit {
class WebLayer;
}

namespace content {

// A RenderWidget that hosts a fullscreen pepper plugin. This provides a
// FullscreenContainer that the plugin instance can callback into to e.g.
// invalidate rects.
class RenderWidgetFullscreenPepper :
    public RenderWidgetFullscreen,
    public webkit::ppapi::FullscreenContainer {
 public:
  static RenderWidgetFullscreenPepper* Create(
      int32 opener_id,
      webkit::ppapi::PluginInstance* plugin,
      const GURL& active_url,
      const WebKit::WebScreenInfo& screen_info);

  // pepper::FullscreenContainer API.
  virtual void Invalidate() OVERRIDE;
  virtual void InvalidateRect(const WebKit::WebRect& rect) OVERRIDE;
  virtual void ScrollRect(int dx, int dy, const WebKit::WebRect& rect) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void DidChangeCursor(const WebKit::WebCursorInfo& cursor) OVERRIDE;
  virtual webkit::ppapi::PluginDelegate::PlatformContext3D*
      CreateContext3D() OVERRIDE;
  virtual void ReparentContext(
      webkit::ppapi::PluginDelegate::PlatformContext3D*) OVERRIDE;
  virtual void SetLayer(WebKit::WebLayer* layer) OVERRIDE;

  // IPC::Listener implementation. This overrides the implementation
  // in RenderWidgetFullscreen.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Could be NULL when this widget is closing.
  webkit::ppapi::PluginInstance* plugin() const { return plugin_; }

  MouseLockDispatcher* mouse_lock_dispatcher() const {
    return mouse_lock_dispatcher_.get();
  }

  bool is_compositing() const { return !!layer_; }

 protected:
  RenderWidgetFullscreenPepper(webkit::ppapi::PluginInstance* plugin,
                               const GURL& active_url,
                               const WebKit::WebScreenInfo& screen_info);
  virtual ~RenderWidgetFullscreenPepper();

  // RenderWidget API.
  virtual void WillInitiatePaint() OVERRIDE;
  virtual void DidInitiatePaint() OVERRIDE;
  virtual void DidFlushPaint() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual webkit::ppapi::PluginInstance* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip,
      float* scale_factor) OVERRIDE;
  virtual void OnResize(const gfx::Size& new_size,
                        const gfx::Size& physical_backing_size,
                        float overdraw_bottom_height,
                        const gfx::Rect& resizer_rect,
                        bool is_fullscreen) OVERRIDE;

  // RenderWidgetFullscreen API.
  virtual WebKit::WebWidget* CreateWebWidget() OVERRIDE;

  // RenderWidget overrides.
  virtual GURL GetURLForGraphicsContext3D() OVERRIDE;
  virtual void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;

 private:
  // URL that is responsible for this widget, passed to ggl::CreateViewContext.
  GURL active_url_;

  // The plugin instance this widget wraps.
  webkit::ppapi::PluginInstance* plugin_;

  WebKit::WebLayer* layer_;

  scoped_ptr<MouseLockDispatcher> mouse_lock_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetFullscreenPepper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
