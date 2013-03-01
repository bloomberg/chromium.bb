// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
#define CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/renderer/mouse_lock_dispatcher.h"
#include "content/renderer/pepper/pepper_parent_context_provider.h"
#include "content/renderer/render_widget_fullscreen.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWidget.h"
#include "webkit/plugins/ppapi/fullscreen_container.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

}  // namespace ppapi
}  // namespace webkit

namespace content {
class WebGraphicsContext3DCommandBufferImpl;

// A RenderWidget that hosts a fullscreen pepper plugin. This provides a
// FullscreenContainer that the plugin instance can callback into to e.g.
// invalidate rects.
class RenderWidgetFullscreenPepper :
    public RenderWidgetFullscreen,
    public webkit::ppapi::FullscreenContainer,
    public PepperParentContextProvider,
    public WebGraphicsContext3DSwapBuffersClient {
 public:
  static RenderWidgetFullscreenPepper* Create(
      int32 opener_id,
      webkit::ppapi::PluginInstance* plugin,
      const GURL& active_url,
      const WebKit::WebScreenInfo& screen_info);

  // WebGraphicscontext3DSwapBuffersClient implementation
  virtual void OnViewContextSwapBuffersPosted() OVERRIDE;
  virtual void OnViewContextSwapBuffersComplete() OVERRIDE;
  virtual void OnViewContextSwapBuffersAborted() OVERRIDE;

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

  // IPC::Listener implementation. This overrides the implementation
  // in RenderWidgetFullscreen.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  WebGraphicsContext3DCommandBufferImpl* context() const { return context_; }
  void SwapBuffers();

  // Could be NULL when this widget is closing.
  webkit::ppapi::PluginInstance* plugin() const { return plugin_; }

  MouseLockDispatcher* mouse_lock_dispatcher() const {
    return mouse_lock_dispatcher_.get();
  }

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
                        const gfx::Rect& resizer_rect,
                        bool is_fullscreen) OVERRIDE;

  // RenderWidgetFullscreen API.
  virtual WebKit::WebWidget* CreateWebWidget() OVERRIDE;

  // RenderWidget overrides.
  virtual bool SupportsAsynchronousSwapBuffers() OVERRIDE;
  virtual void Composite() OVERRIDE;

 private:
  // Creates the GL context for compositing.
  void CreateContext();

  // Initialize the GL states and resources for compositing.
  bool InitContext();

  // Checks (and returns) whether accelerated compositing should be on or off,
  // and notify the browser.
  bool CheckCompositing();

  // Implementation of PepperParentContextProvider.
  virtual WebGraphicsContext3DCommandBufferImpl*
      GetParentContextForPlatformContext3D() OVERRIDE;

  // URL that is responsible for this widget, passed to ggl::CreateViewContext.
  GURL active_url_;

  // The plugin instance this widget wraps.
  webkit::ppapi::PluginInstance* plugin_;

  // GL context for compositing.
  WebGraphicsContext3DCommandBufferImpl* context_;
  unsigned int buffer_;
  unsigned int program_;

  base::WeakPtrFactory<RenderWidgetFullscreenPepper> weak_ptr_factory_;

  scoped_ptr<MouseLockDispatcher> mouse_lock_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetFullscreenPepper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_FULLSCREEN_PEPPER_H_
