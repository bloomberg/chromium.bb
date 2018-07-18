// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
#define CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/pepper/fullscreen_container.h"
#include "content/renderer/render_widget.h"
#include "third_party/blink/public/web/web_widget.h"
#include "url/gurl.h"

namespace cc {
class Layer;
}

namespace content {
class CompositorDependencies;
class PepperPluginInstanceImpl;

// A RenderWidget that hosts a fullscreen pepper plugin. This provides a
// FullscreenContainer that the plugin instance can callback into to e.g.
// invalidate rects.
class RenderWidgetFullscreenPepper : public RenderWidget,
                                     public FullscreenContainer {
 public:
  static RenderWidgetFullscreenPepper* Create(
      int32_t routing_id,
      RenderWidget::ShowCallback show_callback,
      CompositorDependencies* compositor_deps,
      PepperPluginInstanceImpl* plugin,
      const GURL& active_url,
      const ScreenInfo& screen_info,
      mojom::WidgetRequest widget_request);

  // pepper::FullscreenContainer API.
  void Invalidate() override;
  void InvalidateRect(const blink::WebRect& rect) override;
  void ScrollRect(int dx, int dy, const blink::WebRect& rect) override;
  void Destroy() override;
  void PepperDidChangeCursor(const blink::WebCursorInfo& cursor) override;
  void SetLayer(cc::Layer* layer) override;

  // RenderWidget overrides.
  bool OnMessageReceived(const IPC::Message& msg) override;

  // Could be NULL when this widget is closing.
  PepperPluginInstanceImpl* plugin() const { return plugin_; }

  MouseLockDispatcher* mouse_lock_dispatcher() const {
    return mouse_lock_dispatcher_.get();
  }

 protected:
  RenderWidgetFullscreenPepper(int32_t routing_id,
                               CompositorDependencies* compositor_deps,
                               PepperPluginInstanceImpl* plugin,
                               const GURL& active_url,
                               const ScreenInfo& screen_info,
                               mojom::WidgetRequest widget_request);
  ~RenderWidgetFullscreenPepper() override;

  // RenderWidget API.
  void DidInitiatePaint() override;
  void Close() override;
  void OnSynchronizeVisualProperties(
      const VisualProperties& visual_properties) override;

  // RenderWidget overrides.
  GURL GetURLForGraphicsContext3D() override;

 private:
  void UpdateLayerBounds();

  // URL that is responsible for this widget, passed to ggl::CreateViewContext.
  GURL active_url_;

  // The plugin instance this widget wraps.
  PepperPluginInstanceImpl* plugin_;

  cc::Layer* layer_;

  std::unique_ptr<MouseLockDispatcher> mouse_lock_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetFullscreenPepper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
