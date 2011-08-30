// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
#define CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_

#include "base/task.h"
#include "content/renderer/render_widget_fullscreen.h"
#include "content/renderer/gpu/renderer_gl_context.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"
#include "webkit/plugins/ppapi/fullscreen_container.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

}  // namespace ppapi
}  // namespace webkit

// A RenderWidget that hosts a fullscreen pepper plugin. This provides a
// FullscreenContainer that the plugin instance can callback into to e.g.
// invalidate rects.
class RenderWidgetFullscreenPepper : public RenderWidgetFullscreen,
                                     public webkit::ppapi::FullscreenContainer {
 public:
  static RenderWidgetFullscreenPepper* Create(
      int32 opener_id,
      RenderThreadBase* render_thread,
      webkit::ppapi::PluginInstance* plugin,
      const GURL& active_url);

  // pepper::FullscreenContainer API.
  virtual void Invalidate();
  virtual void InvalidateRect(const WebKit::WebRect& rect);
  virtual void ScrollRect(int dx, int dy, const WebKit::WebRect& rect);
  virtual void Destroy();
  virtual void DidChangeCursor(const WebKit::WebCursorInfo& cursor);
  virtual webkit::ppapi::PluginDelegate::PlatformContext3D* CreateContext3D();

  RendererGLContext* context() const { return context_; }
  void SwapBuffers();

 protected:
  RenderWidgetFullscreenPepper(RenderThreadBase* render_thread,
                               webkit::ppapi::PluginInstance* plugin,
                               const GURL& active_url);
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

  // RenderWidget overrides.
  virtual bool SupportsAsynchronousSwapBuffers() OVERRIDE;

 private:
  // Creates the GL context for compositing.
  void CreateContext();

  // Initialize the GL states and resources for compositing.
  bool InitContext();

  // Checks (and returns) whether accelerated compositing should be on or off,
  // and notify the browser.
  bool CheckCompositing();

  // Called when the compositing context gets lost.
  void OnLostContext(RendererGLContext::ContextLostReason);

  // Binding of RendererGLContext swapbuffers callback to
  // RenderWidget::OnSwapBuffersCompleted.
  void OnSwapBuffersCompleteByRendererGLContext();

  // URL that is responsible for this widget, passed to ggl::CreateViewContext.
  GURL active_url_;

  // The plugin instance this widget wraps.
  webkit::ppapi::PluginInstance* plugin_;

  // GL context for compositing.
  RendererGLContext* context_;
  unsigned int buffer_;
  unsigned int program_;

  ScopedRunnableMethodFactory<RenderWidgetFullscreenPepper> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetFullscreenPepper);
};

#endif  // CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
