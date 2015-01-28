// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_AURA_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_AURA_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/content_export.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/wm/public/drag_drop_delegate.h"

namespace aura {
class Window;
}

namespace ui {
class DropTargetEvent;
}

namespace content {
class GestureNavSimple;
class OverscrollNavigationOverlay;
class RenderWidgetHostImpl;
class RenderWidgetHostViewAura;
class ShadowLayerDelegate;
class TouchEditableImplAura;
class WebContentsViewDelegate;
class WebContentsImpl;
class WebDragDestDelegate;

class WebContentsViewAura
    : public WebContentsView,
      public RenderViewHostDelegateView,
      public OverscrollControllerDelegate,
      public ui::ImplicitAnimationObserver,
      public aura::WindowDelegate,
      public aura::client::DragDropDelegate,
      public aura::WindowObserver {
 public:
  WebContentsViewAura(WebContentsImpl* web_contents,
                      WebContentsViewDelegate* delegate);

  CONTENT_EXPORT void SetTouchEditableForTest(
      TouchEditableImplAura* touch_editable);

 private:
  class WindowObserver;

  ~WebContentsViewAura() override;

  void SizeChangedCommon(const gfx::Size& size);

  void EndDrag(blink::WebDragOperationsMask ops);

  void InstallOverscrollControllerDelegate(RenderWidgetHostViewAura* view);

  // Creates and sets up the overlay window that will be displayed during the
  // overscroll gesture.
  void PrepareOverscrollWindow();

  // Sets up the content window in preparation for starting an overscroll
  // gesture.
  void PrepareContentWindowForOverscroll();

  // Resets any in-progress animation for the overscroll gesture. Note that this
  // doesn't immediately reset the internal states; that happens after an
  // animation.
  void ResetOverscrollTransform();

  // Completes the navigation in response to a completed overscroll gesture.
  // The navigation happens after an animation (either the overlay window
  // animates in, or the content window animates out).
  void CompleteOverscrollNavigation(OverscrollMode mode);

  // Returns the window that should be animated for the overscroll gesture.
  // (note that during the overscroll gesture, either the overlay window or the
  // content window can be animated).
  aura::Window* GetWindowToAnimateForOverscroll();

  // Returns the amount the animating window should be translated in response to
  // the overscroll gesture.
  gfx::Vector2dF GetTranslationForOverscroll(float delta_x, float delta_y);

  // A window showing the screenshot is overlayed during a navigation triggered
  // by overscroll. This function sets this up.
  void PrepareOverscrollNavigationOverlay();

  // Changes the brightness of the layer depending on the amount of horizontal
  // overscroll (|delta_x|, in pixels).
  void UpdateOverscrollWindowBrightness(float delta_x);

  void AttachTouchEditableToRenderView();

  void OverscrollUpdateForWebContentsDelegate(float delta_y);

  // Overridden from WebContentsView:
  gfx::NativeView GetNativeView() const override;
  gfx::NativeView GetContentNativeView() const override;
  gfx::NativeWindow GetTopLevelNativeWindow() const override;
  void GetContainerBounds(gfx::Rect* out) const override;
  void SizeContents(const gfx::Size& size) override;
  void Focus() override;
  void SetInitialFocus() override;
  void StoreFocus() override;
  void RestoreFocus() override;
  DropData* GetDropData() const override;
  gfx::Rect GetViewBounds() const override;
  void CreateView(const gfx::Size& initial_size,
                  gfx::NativeView context) override;
  RenderWidgetHostViewBase* CreateViewForWidget(
      RenderWidgetHost* render_widget_host,
      bool is_guest_view_hack) override;
  RenderWidgetHostViewBase* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) override;
  void SetPageTitle(const base::string16& title) override;
  void RenderViewCreated(RenderViewHost* host) override;
  void RenderViewSwappedIn(RenderViewHost* host) override;
  void SetOverscrollControllerEnabled(bool enabled) override;

  // Overridden from RenderViewHostDelegateView:
  void ShowContextMenu(RenderFrameHost* render_frame_host,
                       const ContextMenuParams& params) override;
  void StartDragging(const DropData& drop_data,
                     blink::WebDragOperationsMask operations,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const DragEventSourceInfo& event_info) override;
  void UpdateDragCursor(blink::WebDragOperation operation) override;
  void GotFocus() override;
  void TakeFocus(bool reverse) override;
  void ShowDisambiguationPopup(
      const gfx::Rect& target_rect,
      const SkBitmap& zoomed_bitmap,
      const base::Callback<void(ui::GestureEvent*)>& gesture_cb,
      const base::Callback<void(ui::MouseEvent*)>& mouse_cb) override;
  void HideDisambiguationPopup() override;

  // Overridden from OverscrollControllerDelegate:
  gfx::Rect GetVisibleBounds() const override;
  bool OnOverscrollUpdate(float delta_x, float delta_y) override;
  void OnOverscrollComplete(OverscrollMode overscroll_mode) override;
  void OnOverscrollModeChange(OverscrollMode old_mode,
                              OverscrollMode new_mode) override;

  // Overridden from ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
  ui::TextInputClient* GetFocusedTextInputClient() override;
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnDeviceScaleFactorChanged(float device_scale_factor) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowTargetVisibilityChanged(bool visible) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(gfx::Path* mask) const override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overridden from aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

  // Overridden from aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // Update the web contents visiblity.
  void UpdateWebContentsVisibility(bool visible);

  scoped_ptr<aura::Window> window_;

  // The window that shows the screenshot of the history page during an
  // overscroll navigation gesture.
  scoped_ptr<aura::Window> overscroll_window_;

  scoped_ptr<WindowObserver> window_observer_;

  // The WebContentsImpl whose contents we display.
  WebContentsImpl* web_contents_;

  scoped_ptr<WebContentsViewDelegate> delegate_;

  blink::WebDragOperationsMask current_drag_op_;

  scoped_ptr<DropData> current_drop_data_;

  WebDragDestDelegate* drag_dest_delegate_;

  // We keep track of the render view host we're dragging over.  If it changes
  // during a drag, we need to re-send the DragEnter message.  WARNING:
  // this pointer should never be dereferenced.  We only use it for comparing
  // pointers.
  void* current_rvh_for_drag_;

  bool overscroll_change_brightness_;

  // The overscroll gesture currently in progress.
  OverscrollMode current_overscroll_gesture_;

  // This is the completed overscroll gesture. This is used for the animation
  // callback that happens in response to a completed overscroll gesture.
  OverscrollMode completed_overscroll_gesture_;

  // This manages the overlay window that shows the screenshot during a history
  // navigation triggered by the overscroll gesture.
  scoped_ptr<OverscrollNavigationOverlay> navigation_overlay_;

  scoped_ptr<ShadowLayerDelegate> overscroll_shadow_;

  scoped_ptr<TouchEditableImplAura> touch_editable_;
  scoped_ptr<GestureNavSimple> gesture_nav_simple_;

  // On Windows we can run into problems if resources get released within the
  // initialization phase while the content (and its dimensions) are not known.
  bool is_or_was_visible_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_AURA_H_
