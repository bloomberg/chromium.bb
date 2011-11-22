// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
#pragma once

#include <map>

#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/common/content_export.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/compositor/compositor_observer.h"
#include "webkit/glue/webcursor.h"

namespace WebKit {
class WebTouchEvent;
}

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
class AcceleratedSurfaceContainerLinux;
#endif

class CONTENT_EXPORT RenderWidgetHostViewAura
    : NON_EXPORTED_BASE(public RenderWidgetHostView),
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
      public ui::CompositorObserver,
#endif
      public aura::WindowDelegate {
 public:
  explicit RenderWidgetHostViewAura(RenderWidgetHost* host);
  virtual ~RenderWidgetHostViewAura();

  // TODO(derat): Add an abstract RenderWidgetHostView::InitAsChild() method and
  // update callers: http://crbug.com/102450.
  void InitAsChild();

  // Overridden from RenderWidgetHostView:
  virtual void InitAsPopup(RenderWidgetHostView* parent_host_view,
                           const gfx::Rect& pos) OVERRIDE;
  virtual void InitAsFullscreen(
      RenderWidgetHostView* reference_host_view) OVERRIDE;
  virtual RenderWidgetHost* GetRenderWidgetHost() const OVERRIDE;
  virtual void DidBecomeSelected() OVERRIDE;
  virtual void WasHidden() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& rect) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeViewId GetNativeViewId() const OVERRIDE;
  virtual void MovePluginWindows(
      const std::vector<webkit::npapi::WebPluginGeometry>& moves) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void Blur() OVERRIDE;
  virtual bool HasFocus() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsShowing() OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;
  virtual void UpdateCursor(const WebCursor& cursor) OVERRIDE;
  virtual void SetIsLoading(bool is_loading) OVERRIDE;
  virtual void TextInputStateChanged(ui::TextInputType type,
                                     bool can_compose_inline) OVERRIDE;
  virtual void ImeCancelComposition() OVERRIDE;
  virtual void DidUpdateBackingStore(
      const gfx::Rect& scroll_rect, int scroll_dx, int scroll_dy,
      const std::vector<gfx::Rect>& copy_rects) OVERRIDE;
  virtual void RenderViewGone(base::TerminationStatus status,
                              int error_code) OVERRIDE;
  virtual void Destroy() OVERRIDE;
  virtual void SetTooltipText(const string16& tooltip_text) OVERRIDE;
  virtual BackingStore* AllocBackingStore(const gfx::Size& size) OVERRIDE;
  virtual void OnAcceleratedCompositingStateChange() OVERRIDE;
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  virtual void AcceleratedSurfaceNew(
      int32 width,
      int32 height,
      uint64* surface_id,
      TransportDIB::Handle* surface_handle) OVERRIDE;
  virtual void AcceleratedSurfaceBuffersSwapped(
      uint64 surface_id,
      int32 route_id,
      int gpu_host_id) OVERRIDE;
  virtual void AcceleratedSurfaceRelease(uint64 surface_id) OVERRIDE;
#endif
  virtual void SetBackground(const SkBitmap& background) OVERRIDE;
  virtual void GetScreenInfo(WebKit::WebScreenInfo* results) OVERRIDE;
  virtual gfx::Rect GetRootWindowBounds() OVERRIDE;
  virtual void SetVisuallyDeemphasized(const SkColor* color,
                                       bool animate) OVERRIDE;
  virtual void UnhandledWheelEvent(
      const WebKit::WebMouseWheelEvent& event) OVERRIDE;
  virtual void SetHasHorizontalScrollbar(
      bool has_horizontal_scrollbar) OVERRIDE;
  virtual void SetScrollOffsetPinning(
      bool is_pinned_to_left, bool is_pinned_to_right) OVERRIDE;
  virtual gfx::PluginWindowHandle GetCompositingSurface() OVERRIDE;
  virtual bool LockMouse() OVERRIDE;
  virtual void UnlockMouse() OVERRIDE;

  // Overridden from aura::WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual bool OnKeyEvent(aura::KeyEvent* event) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE;
  virtual bool OnMouseEvent(aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus OnTouchEvent(aura::TouchEvent* event) OVERRIDE;
  virtual bool ShouldActivate(aura::Event* event) OVERRIDE;
  virtual void OnActivated() OVERRIDE;
  virtual void OnLostActive() OVERRIDE;
  virtual void OnCaptureLost() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnWindowDestroying() OVERRIDE;
  virtual void OnWindowDestroyed() OVERRIDE;
  virtual void OnWindowVisibilityChanged(bool visible) OVERRIDE;

 private:
#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  // Overridden from ui::CompositorObserver:
  virtual void OnCompositingEnded(ui::Compositor* compositor) OVERRIDE;
#endif

  void UpdateCursorIfOverSelf();

  // The model object.
  RenderWidgetHost* host_;

  aura::Window* window_;

  // Is this a fullscreen view?
  bool is_fullscreen_;

  // Our parent host view, if this is a popup.  NULL otherwise.
  RenderWidgetHostViewAura* popup_parent_host_view_;

  // True when content is being loaded. Used to show an hourglass cursor.
  bool is_loading_;

  // The cursor for the page. This is passed up from the renderer.
  WebCursor current_cursor_;

  // The touch-event. Its touch-points are updated as necessary. A new
  // touch-point is added from an ET_TOUCH_PRESSED event, and a touch-point is
  // removed from the list on an ET_TOUCH_RELEASED event.
  WebKit::WebTouchEvent touch_event_;

  // Current tooltip text.
  string16 tooltip_;

#if defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
  std::vector< base::Callback<void(void)> > on_compositing_ended_callbacks_;

  std::map<uint64, scoped_refptr<AcceleratedSurfaceContainerLinux> >
      accelerated_surface_containers_;
#endif

  bool skip_schedule_paint_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostViewAura);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_VIEW_AURA_H_
