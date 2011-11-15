// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_VIEWS_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "views/widget/widget.h"

class ConstrainedWindowGtk;
class NativeTabContentsView;
class RenderViewContextMenuViews;
class SkBitmap;
struct WebDropData;
namespace gfx {
class Point;
class Size;
}
namespace views {
class Widget;
}

// Views-specific implementation of the TabContentsView.
class TabContentsViewViews : public views::Widget,
                             public TabContentsView,
                             public internal::NativeTabContentsViewDelegate {
 public:
  // The corresponding TabContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit TabContentsViewViews(TabContents* tab_contents);
  virtual ~TabContentsViewViews();

  // Intermediate code to pass comiplation. This will be removed as a
  // part of ConstraintWindow change (http://codereview.chromium.org/7631049).
  void AttachConstrainedWindow(ConstrainedWindowGtk* constrained_window);
  void RemoveConstrainedWindow(ConstrainedWindowGtk* constrained_window);

  // Reset the native parent of this view to NULL.  Unparented windows should
  // not receive any messages.
  virtual void Unparent();

  NativeTabContentsView* native_tab_contents_view() const {
    return native_tab_contents_view_;
  }

  // Overridden from TabContentsView:
  virtual void CreateView(const gfx::Size& initial_size) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* host) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual bool IsDoingDrag() const OVERRIDE;
  virtual void CancelDragAndCloseTab() OVERRIDE;
  virtual bool IsEventTracking() const;
  virtual void CloseTabAfterEventTracking();
  virtual void GetViewBounds(gfx::Rect* out) const OVERRIDE;
  virtual void InstallOverlayView(gfx::NativeView view) OVERRIDE;
  virtual void RemoveOverlayView() OVERRIDE;

  // Implementation of RenderViewHostDelegate::View.
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params) OVERRIDE;
  virtual void CreateNewWidget(int route_id,
                               WebKit::WebPopupType popup_type) OVERRIDE;
  virtual void CreateNewFullscreenWidget(int route_id) OVERRIDE;
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) OVERRIDE;
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) OVERRIDE;
  virtual void ShowCreatedFullscreenWidget(int route_id) OVERRIDE;
  virtual void ShowContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask operations,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) OVERRIDE;
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;

 private:
  // Overridden from internal::NativeTabContentsViewDelegate:
  virtual TabContents* GetTabContents() OVERRIDE;
  virtual bool IsShowingSadTab() const OVERRIDE;
  virtual void OnNativeTabContentsViewShown() OVERRIDE;
  virtual void OnNativeTabContentsViewHidden() OVERRIDE;
  virtual void OnNativeTabContentsViewSized(const gfx::Size& size) OVERRIDE;
  virtual void OnNativeTabContentsViewWheelZoom(bool zoom_in) OVERRIDE;
  virtual void OnNativeTabContentsViewMouseDown() OVERRIDE;
  virtual void OnNativeTabContentsViewMouseMove(bool motion) OVERRIDE;
  virtual void OnNativeTabContentsViewDraggingEnded() OVERRIDE;
  virtual views::internal::NativeWidgetDelegate*
      AsNativeWidgetDelegate() OVERRIDE;

  // Overridden from views::Widget:
  virtual views::FocusManager* GetFocusManager() OVERRIDE;
  virtual const views::FocusManager* GetFocusManager() const OVERRIDE;
  virtual void OnNativeWidgetVisibilityChanged(bool visible) OVERRIDE;
  virtual void OnNativeWidgetSizeChanged(const gfx::Size& new_size) OVERRIDE;

  // A helper method for closing the tab.
  void CloseTab();

  // Windows events ------------------------------------------------------------

  // Handles notifying the TabContents and other operations when the window was
  // shown or hidden.
  void WasHidden();
  void WasShown();

  // Handles resizing of the contents. This will notify the RenderWidgetHostView
  // of the change, reposition popups, and the find in page bar.
  void WasSized(const gfx::Size& size);

  // TODO(brettw) comment these. They're confusing.
  void WheelZoom(int distance);

  // ---------------------------------------------------------------------------

  // The TabContents whose contents we display.
  TabContents* tab_contents_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  NativeTabContentsView* native_tab_contents_view_;

  // The id used in the ViewStorage to store the last focused view.
  int last_focused_view_storage_id_;

  // The context menu. Callbacks are asynchronous so we need to keep it around.
  scoped_ptr<RenderViewContextMenuViews> context_menu_;

  // Set to true if we want to close the tab after the system drag operation
  // has finished.
  bool close_tab_after_drag_ends_;

  // Used to close the tab after the stack has unwound.
  base::OneShotTimer<TabContentsViewViews> close_tab_timer_;

  // The FocusManager associated with this tab.  Stored as it is not directly
  // accessible when un-parented.
  mutable const views::FocusManager* focus_manager_;

  // The overlaid view. Owned by the caller of |InstallOverlayView|; this is a
  // weak reference.
  views::Widget* overlaid_view_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_VIEWS_H_
