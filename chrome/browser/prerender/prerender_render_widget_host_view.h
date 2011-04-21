// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_RENDER_WIDGET_HOST_VIEW_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_RENDER_WIDGET_HOST_VIEW_H_
#pragma once

#include <vector>

#include "content/browser/renderer_host/render_widget_host_view.h"

namespace prerender {

class PrerenderContents;

// A PrerenderRenderWidgetHostView acts as the "View" of prerendered web pages,
// as they don't have an actual window (visible or hidden) until swapped into
// a tab.
class PrerenderRenderWidgetHostView : public RenderWidgetHostView {
 public:
  PrerenderRenderWidgetHostView(RenderWidgetHost* widget,
                                PrerenderContents* prerender_contents);
  virtual ~PrerenderRenderWidgetHostView();

  // Initializes |this| using the bounds from |view|, which must be non-NULL.
  void Init(RenderWidgetHostView* view);

  // Implementation of RenderWidgetHostView:
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen() OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void DidBecomeSelected() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() OVERRIDE;
  virtual void MovePluginWindows(
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual bool HasFocus() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void ImeUpdateTextInputState(WebKit::WebTextInputType type,
                                       const gfx::Rect& caret_rect) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
      const std::vector<gfx::Rect>& copy_rects) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void WillDestroyRenderWidget(RenderWidgetHost* rwh) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const std::wstring& tooltip_text) OVERRIDE;
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;

#if defined(OS_MACOSX)
  virtual void SetTakesFocusOnlyOnMouseDown(bool flag) OVERRIDE;
  virtual gfx::Rect GetViewCocoaBounds() const OVERRIDE;
  virtual gfx::Rect GetRootWindowRect() OVERRIDE;
  virtual void SetActive(bool active) OVERRIDE;
  virtual void SetWindowVisibility(bool visible) OVERRIDE;
  virtual void WindowFrameChanged() OVERRIDE;
  virtual void PluginFocusChanged(bool focused, int plugin_id) OVERRIDE;
  virtual void StartPluginIme() OVERRIDE;
  virtual bool PostProcessEventForPluginIme(
      const NativeWebKeyboardEvent& event) OVERRIDE;

  virtual gfx::PluginWindowHandle AllocateFakePluginWindowHandle(
      bool opaque, bool root) OVERRIDE;
  virtual void DestroyFakePluginWindowHandle(
      gfx::PluginWindowHandle window) OVERRIDE;
  virtual void AcceleratedSurfaceSetIOSurface(
      gfx::PluginWindowHandle window,
      int32 width,
      int32 height,
      uint64 io_surface_identifier) OVERRIDE;
  virtual void AcceleratedSurfaceSetTransportDIB(
      gfx::PluginWindowHandle window,
      int32 width,
      int32 height,
      TransportDIB::Handle transport_dib) OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      gfx::PluginWindowHandle window,
      uint64 surface_id,
      int renderer_id,
      int32 route_id,
      int gpu_host_id,
      uint64 swap_buffers_count) OVERRIDE;
  virtual void GpuRenderingStateDidChange() OVERRIDE;
#endif

#if defined(TOOLKIT_USES_GTK)
  virtual void CreatePluginContainer(gfx::PluginWindowHandle id) OVERRIDE;
  virtual void DestroyPluginContainer(gfx::PluginWindowHandle id) OVERRIDE;
  virtual void AcceleratedCompositingActivated(bool activated) OVERRIDE;
#endif

#if defined(OS_WIN)
  virtual void WillWmDestroy() OVERRIDE;
  virtual void ShowCompositorHostWindow(bool show) OVERRIDE;
#endif

  virtual gfx::PluginWindowHandle GetCompositingSurface() OVERRIDE;
  virtual void SetVisuallyDeemphasized(const SkColor* color,
                                       bool animate) OVERRIDE;
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;

  virtual bool ContainsNativeView(gfx::NativeView native_view) const OVERRIDE;

 private:
  RenderWidgetHost* render_widget_host_;
  // Need this to cancel prerendering in some cases.
  PrerenderContents* prerender_contents_;

  gfx::Rect bounds_;
#if defined(OS_MACOSX)
  gfx::Rect cocoa_view_bounds_;
  gfx::Rect root_window_rect_;
#endif  // defined(OS_MACOSX)

  DISALLOW_COPY_AND_ASSIGN(PrerenderRenderWidgetHostView);
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_RENDER_WIDGET_HOST_VIEW_H_
