// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
#define CHROME_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_

#include "chrome/renderer/render_widget_fullscreen.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"
#include "webkit/plugins/ppapi/fullscreen_container.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

}  // namespace ppapi
}  // namespace webkit

namespace ggl {
class Context;
}  // namespace ggl

// A RenderWidget that hosts a fullscreen pepper plugin. This provides a
// FullscreenContainer that the plugin instance can callback into to e.g.
// invalidate rects.
class RenderWidgetFullscreenPepper : public RenderWidgetFullscreen,
                                     public webkit::ppapi::FullscreenContainer {
 public:
  static RenderWidgetFullscreenPepper* Create(
      int32 opener_id,
      RenderThreadBase* render_thread,
      webkit::ppapi::PluginInstance* plugin);

  // pepper::FullscreenContainer API.
  virtual void Invalidate();
  virtual void InvalidateRect(const WebKit::WebRect& rect);
  virtual void ScrollRect(int dx, int dy, const WebKit::WebRect& rect);
  virtual void Destroy();
  virtual webkit::ppapi::PluginDelegate::PlatformContext3D* CreateContext3D();

  ggl::Context* context() const { return context_; }

 protected:
  RenderWidgetFullscreenPepper(RenderThreadBase* render_thread,
                               webkit::ppapi::PluginInstance* plugin);
  virtual ~RenderWidgetFullscreenPepper();

  // RenderWidget API.
  virtual void DidInitiatePaint();
  virtual void DidFlushPaint();
  virtual void Close();
  virtual webkit::ppapi::PluginInstance* GetBitmapForOptimizedPluginPaint(
      const gfx::Rect& paint_bounds,
      TransportDIB** dib,
      gfx::Rect* location,
      gfx::Rect* clip);
  virtual void OnResize(const gfx::Size& new_size,
                        const gfx::Rect& resizer_rect);

  // RenderWidgetFullscreen API.
  virtual WebKit::WebWidget* CreateWebWidget();

 private:
  // Creates the GL context for compositing.
  void CreateContext();

  // Destroys the GL context for compositing.
  void DestroyContext();

  // Initialize the GL states and resources for compositing.
  bool InitContext();

  // Checks (and returns) whether accelerated compositing should be on or off,
  // and notify the browser.
  bool CheckCompositing();

  // The plugin instance this widget wraps.
  webkit::ppapi::PluginInstance* plugin_;

  // GL context for compositing.
  ggl::Context* context_;
  unsigned int buffer_;
  unsigned int program_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetFullscreenPepper);
};

#endif  // CHROME_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
