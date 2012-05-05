// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "base/win/win_util.h"
#include "content/browser/web_contents/web_contents_view_helper.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/win/window_impl.h"

class RenderWidgetHostViewWin;
class WebDragDest;
class WebContentsDragWin;

namespace content {
class WebContentsViewDelegate;
}

// An implementation of WebContentsView for Windows.
class CONTENT_EXPORT WebContentsViewWin : public content::WebContentsView,
                                          public ui::WindowImpl {
 public:
  WebContentsViewWin(WebContentsImpl* web_contents,
                     content::WebContentsViewDelegate* delegate);
  virtual ~WebContentsViewWin();

  BEGIN_MSG_MAP_EX(WebContentsViewWin)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_WINDOWPOSCHANGED, OnWindowPosChanged)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnMouseDown)
    MESSAGE_HANDLER(WM_MBUTTONDOWN, OnMouseDown)
    MESSAGE_HANDLER(WM_RBUTTONDOWN, OnMouseDown)
    MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
    MESSAGE_HANDLER(base::win::kReflectedMessage, OnReflectedMessage)
    // Hacks for old ThinkPad touchpads/scroll points.
    MESSAGE_HANDLER(WM_NCCALCSIZE, OnNCCalcSize)
    MESSAGE_HANDLER(WM_HSCROLL, OnScroll)
    MESSAGE_HANDLER(WM_VSCROLL, OnScroll)
    MESSAGE_HANDLER(WM_SIZE, OnSize)
  END_MSG_MAP()

  // Overridden from WebContentsView:
  virtual void CreateView(const gfx::Size& initial_size) OVERRIDE;
  virtual content::RenderWidgetHostView* CreateViewForWidget(
      content::RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect *out) const OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void RenderViewCreated(content::RenderViewHost* host) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual bool IsDoingDrag() const OVERRIDE;
  virtual void CancelDragAndCloseTab() OVERRIDE;
  virtual WebDropData* GetDropData() const OVERRIDE;
  virtual bool IsEventTracking() const OVERRIDE;
  virtual void CloseTabAfterEventTracking() OVERRIDE;
  virtual void GetViewBounds(gfx::Rect* out) const OVERRIDE;

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
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;
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

  WebContentsImpl* web_contents() const { return web_contents_; }

 private:
  void EndDragging();
  void CloseTab();

  LRESULT OnDestroy(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnWindowPosChanged(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnMouseDown(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnMouseMove(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnReflectedMessage(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnNCCalcSize(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnScroll(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnSize(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

  gfx::Size initial_size_;

  // The WebContentsImpl whose contents we display.
  WebContentsImpl* web_contents_;

  RenderWidgetHostViewWin* view_;

  scoped_ptr<content::WebContentsViewDelegate> delegate_;

  // The helper object that handles drag destination related interactions with
  // Windows.
  scoped_refptr<WebDragDest> drag_dest_;

  // Used to handle the drag-and-drop.
  scoped_refptr<WebContentsDragWin> drag_handler_;

  // Set to true if we want to close the tab after the system drag operation
  // has finished.
  bool close_tab_after_drag_ends_;

  // Used to close the tab after the stack has unwound.
  base::OneShotTimer<WebContentsViewWin> close_tab_timer_;

  // Common implementations of some WebContentsView methods.
  WebContentsViewHelper web_contents_view_helper_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewWin);
};

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_
