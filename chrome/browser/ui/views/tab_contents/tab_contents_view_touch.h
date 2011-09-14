// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_TOUCH_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_TOUCH_H_
#pragma once

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/tab_contents/render_view_host_delegate_helper.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "ui/gfx/size.h"
#include "views/view.h"

class ConstrainedWindowGtk;
typedef struct _GtkFloatingContainer GtkFloatingContainer;
class RenderViewContextMenuViews;
class SadTabView;
class SkBitmap;
class TabContentsDragSource;
class WebDragDestGtk;

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class MouseEvent;
class NativeViewHost;
}  // namespace views

// Touch-specific implementation of the TabContentsView for the touch UI.
class TabContentsViewTouch : public TabContentsView, public views::View {
 public:
  // Internal class name
  static const char kViewClassName[];

  // The corresponding TabContents is passed in the constructor, and manages our
  // lifetime. This doesn't need to be the case, but is this way currently
  // because that's what was easiest when they were split.
  explicit TabContentsViewTouch(TabContents* tab_contents);
  virtual ~TabContentsViewTouch();

  // Unlike Windows, ConstrainedWindows need to collaborate with the
  // TabContentsViewTouch to position the dialogs.
  void AttachConstrainedWindow(ConstrainedWindowGtk* constrained_window);
  void RemoveConstrainedWindow(ConstrainedWindowGtk* constrained_window);

  // TabContentsView implementation
  virtual void CreateView(const gfx::Size& initial_size) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect* out) const OVERRIDE;
  virtual void SetPageTitle(const std::wstring& title) OVERRIDE;
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
  virtual bool IsEventTracking() const OVERRIDE;
  virtual void CloseTabAfterEventTracking() OVERRIDE;
  virtual void GetViewBounds(gfx::Rect* out) const OVERRIDE;

  // views::View implementation
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

  // Backend implementation of RenderViewHostDelegate::View.
  virtual void CreateNewWindow(
      int route_id,
      const ViewHostMsg_CreateWindow_Params& params);
  virtual void CreateNewWidget(int route_id, WebKit::WebPopupType popup_type);
  virtual void CreateNewFullscreenWidget(int route_id);
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture);
  virtual void ShowCreatedWidget(int route_id, const gfx::Rect& initial_pos);
  virtual void ShowCreatedFullscreenWidget(int route_id);
  virtual void ShowContextMenu(const ContextMenuParams& params);
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask ops_allowed,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) OVERRIDE;
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;
  virtual void VisibilityChanged(views::View *, bool is_visible) OVERRIDE;

 private:
  // Signal handlers -----------------------------------------------------------

  // Handles notifying the TabContents and other operations when the window was
  // shown or hidden.
  void WasHidden();
  void WasShown();

  // Handles resizing of the contents. This will notify the RenderWidgetHostView
  // of the change, reposition popups, and the find in page bar.
  void WasSized(const gfx::Size& size);

  // For any floating views (ConstrainedDialogs) this function centers them
  // within this view. It's called whem a ConstrainedDialog is attached and
  // when this view is resized.
  void SetFloatingPosition(const gfx::Size& size);

  // The TabContents whose contents we display.
  TabContents* tab_contents_;

  // Common implementations of some RenderViewHostDelegate::View methods.
  RenderViewHostDelegateViewHelper delegate_view_helper_;

  // Used to render the sad tab. This will be non-NULL only when the sad tab is
  // visible.
  scoped_ptr<SadTabView> sad_tab_;

  // Whether to ignore the next CHAR keyboard event.
  bool ignore_next_char_event_;

  // The id used in the ViewStorage to store the last focused view.
  int last_focused_view_storage_id_;

  // The context menu. Callbacks are asynchronous so we need to keep it around.
  scoped_ptr<RenderViewContextMenuViews> context_menu_;

  // Handle drags from this TabContentsView.
  // TODO(anicolao): figure out what's needed for drag'n'drop

  // The event for the last mouse down we handled. We need this for drags.
  GdkEventButton last_mouse_down_;

  // Current size. See comment in NativeWidgetGtk as to why this is cached.
  gfx::Size size_;

  // Each individual UI for constrained dialogs currently displayed. The
  // objects in this vector are owned by the TabContents, not the view.
  std::vector<ConstrainedWindowGtk*> constrained_windows_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsViewTouch);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_TAB_CONTENTS_VIEW_TOUCH_H_
