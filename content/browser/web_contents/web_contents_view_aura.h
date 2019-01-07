// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_AURA_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_AURA_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/visibility.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"

namespace ui {
class DropTargetEvent;
class TouchSelectionController;
}

namespace content {
class GestureNavSimple;
class RenderWidgetHostImpl;
class RenderWidgetHostViewAura;
class TouchSelectionControllerClientAura;
class WebContentsViewDelegate;
class WebContentsImpl;
class WebDragDestDelegate;

class CONTENT_EXPORT WebContentsViewAura
    : public WebContentsView,
      public RenderViewHostDelegateView,
      public aura::WindowDelegate,
      public aura::client::DragDropDelegate {
 public:
  WebContentsViewAura(WebContentsImpl* web_contents,
                      WebContentsViewDelegate* delegate);

  // Allow the WebContentsViewDelegate to be set explicitly.
  void SetDelegateForTesting(WebContentsViewDelegate* delegate);

  // Set a flag to pass nullptr as the parent_view argument to
  // RenderWidgetHostViewAura::InitAsChild().
  void set_init_rwhv_with_null_parent_for_testing(bool set) {
    init_rwhv_with_null_parent_for_testing_ = set;
  }

  using RenderWidgetHostViewCreateFunction =
      RenderWidgetHostViewAura* (*)(RenderWidgetHost*, bool);

  // Used to override the creation of RenderWidgetHostViews in tests.
  static void InstallCreateHookForTests(
      RenderWidgetHostViewCreateFunction create_render_widget_host_view);

 private:
  class WindowObserver;
  class MirrorWindowObserver;

  ~WebContentsViewAura() override;

  void SizeChangedCommon(const gfx::Size& size);

  void EndDrag(RenderWidgetHost* source_rwh, blink::WebDragOperationsMask ops);

  void InstallOverscrollControllerDelegate(RenderWidgetHostViewAura* view);

  ui::TouchSelectionController* GetSelectionController() const;
  TouchSelectionControllerClientAura* GetSelectionControllerClient() const;

  // Returns GetNativeView unless overridden for testing.
  gfx::NativeView GetRenderWidgetHostViewParent() const;

  // Returns whether |target_rwh| is a valid RenderWidgetHost to be dragging
  // over. This enforces that same-page, cross-site drags are not allowed. See
  // crbug.com/666858.
  bool IsValidDragTarget(RenderWidgetHostImpl* target_rwh) const;

  // Called from CreateView() to create |window_|.
  void CreateAuraWindow(aura::Window* context);

  // Computes the view's visibility updates the WebContents accordingly.
  void UpdateWebContentsVisibility();

  // Computes the view's visibility.
  Visibility GetVisibility() const;

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
  void FocusThroughTabTraversal(bool reverse) override;
  DropData* GetDropData() const override;
  gfx::Rect GetViewBounds() const override;
  void CreateView(const gfx::Size& initial_size,
                  gfx::NativeView context) override;
  RenderWidgetHostViewBase* CreateViewForWidget(
      RenderWidgetHost* render_widget_host,
      bool is_guest_view_hack) override;
  RenderWidgetHostViewBase* CreateViewForChildWidget(
      RenderWidgetHost* render_widget_host) override;
  void SetPageTitle(const base::string16& title) override;
  void RenderViewCreated(RenderViewHost* host) override;
  void RenderViewReady() override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void SetOverscrollControllerEnabled(bool enabled) override;

  // Overridden from RenderViewHostDelegateView:
  void ShowContextMenu(RenderFrameHost* render_frame_host,
                       const ContextMenuParams& params) override;
  void StartDragging(const DropData& drop_data,
                     blink::WebDragOperationsMask operations,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override;
  void UpdateDragCursor(blink::WebDragOperation operation) override;
  void GotFocus(RenderWidgetHostImpl* render_widget_host) override;
  void LostFocus(RenderWidgetHostImpl* render_widget_host) override;
  void TakeFocus(bool reverse) override;
#if BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  void ShowPopupMenu(RenderFrameHost* render_frame_host,
                     const gfx::Rect& bounds,
                     int item_height,
                     double item_font_size,
                     int selected_item,
                     const std::vector<MenuItem>& items,
                     bool right_aligned,
                     bool allow_multiple_selection) override;

  void HidePopupMenu() override;
#endif

  // Overridden from aura::WindowDelegate:
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override;
  gfx::NativeCursor GetCursor(const gfx::Point& point) override;
  int GetNonClientComponent(const gfx::Point& point) const override;
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override;
  bool CanFocus() override;
  void OnCaptureLost() override;
  void OnPaint(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowTargetVisibilityChanged(bool visible) override;
  void OnWindowOcclusionChanged(aura::Window::OcclusionState occlusion_state,
                                const SkRegion&) override;
  bool HasHitTestMask() const override;
  void GetHitTestMask(SkPath* mask) const override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // Overridden from aura::client::DragDropDelegate:
  void OnDragEntered(const ui::DropTargetEvent& event) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;

  FRIEND_TEST_ALL_PREFIXES(WebContentsViewAuraTest, EnableDisableOverscroll);

  const bool is_mus_browser_plugin_guest_;

  // NOTE: this is null when running in mus and |is_mus_browser_plugin_guest_|.
  std::unique_ptr<aura::Window> window_;

  std::unique_ptr<WindowObserver> window_observer_;

  std::unique_ptr<MirrorWindowObserver> mirror_window_observer_;

  // The WebContentsImpl whose contents we display.
  WebContentsImpl* web_contents_;

  std::unique_ptr<WebContentsViewDelegate> delegate_;

  blink::WebDragOperationsMask current_drag_op_;

  std::unique_ptr<DropData> current_drop_data_;

  WebDragDestDelegate* drag_dest_delegate_;

  // We keep track of the RenderWidgetHost we're dragging over. If it changes
  // during a drag, we need to re-send the DragEnter message.
  base::WeakPtr<RenderWidgetHostImpl> current_rwh_for_drag_;

  // We also keep track of the ID of the RenderViewHost we're dragging over to
  // avoid sending the drag exited message after leaving the current view.
  GlobalRoutingID current_rvh_for_drag_;

  // We track the IDs of the source RenderProcessHost and RenderViewHost from
  // which the current drag originated. These are used to ensure that drag
  // events do not fire over a cross-site frame (with respect to the source
  // frame) in the same page (see crbug.com/666858). Specifically, the
  // RenderViewHost is used to check the "same page" property, while the
  // RenderProcessHost is used to check the "cross-site" property. Note that the
  // reason the RenderProcessHost is tracked instead of the RenderWidgetHost is
  // so that we still allow drags between non-contiguous same-site frames (such
  // frames will have the same process, but different widgets). Note also that
  // the RenderViewHost may not be in the same process as the RenderProcessHost,
  // since the view corresponds to the page, while the process is specific to
  // the frame from which the drag started.
  int drag_start_process_id_;
  GlobalRoutingID drag_start_view_id_;

  // Responsible for handling gesture-nav and pull-to-refresh UI.
  std::unique_ptr<GestureNavSimple> gesture_nav_simple_;

  bool init_rwhv_with_null_parent_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewAura);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_AURA_H_
