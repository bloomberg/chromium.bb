// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "base/win/win_util.h"
#include "content/common/content_export.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/win/window_impl.h"

class WebDragDest;
class WebContentsDragWin;
class WebContentsImpl;

namespace content {
class RenderWidgetHostViewWin;
class WebContentsViewDelegate;
}

namespace ui {
class HWNDMessageFilter;
}

// An implementation of WebContentsView for Windows.
class CONTENT_EXPORT WebContentsViewWin
    : public content::WebContentsView,
      public content::RenderViewHostDelegateView,
      public ui::WindowImpl {
 public:
  WebContentsViewWin(WebContentsImpl* web_contents,
                     content::WebContentsViewDelegate* delegate);
  virtual ~WebContentsViewWin();

  BEGIN_MSG_MAP_EX(WebContentsViewWin)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_WINDOWPOSCHANGED, OnWindowPosChanged)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnMouseDown)
    MESSAGE_HANDLER(WM_MBUTTONDOWN, OnMouseDown)
    MESSAGE_HANDLER(WM_RBUTTONDOWN, OnMouseDown)
    MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
    MESSAGE_HANDLER(base::win::kReflectedMessage, OnReflectedMessage)
    // Hacks for old ThinkPad touchpads/scroll points.
    MESSAGE_HANDLER(WM_NCCALCSIZE, OnNCCalcSize)
    MESSAGE_HANDLER(WM_NCHITTEST, OnNCHitTest)
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
  virtual gfx::Rect GetViewBounds() const OVERRIDE;

  // Implementation of RenderViewHostDelegateView.
  virtual void ShowContextMenu(
      const content::ContextMenuParams& params,
      const content::ContextMenuSourceType& type) OVERRIDE;
  virtual void ShowPopupMenu(const gfx::Rect& bounds,
                             int item_height,
                             double item_font_size,
                             int selected_item,
                             const std::vector<WebMenuItem>& items,
                             bool right_aligned,
                             bool allow_multiple_selection) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask operations,
                             const gfx::ImageSkia& image,
                             const gfx::Point& image_offset) OVERRIDE;
  virtual void UpdateDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void TakeFocus(bool reverse) OVERRIDE;

  WebContentsImpl* web_contents() const { return web_contents_; }

 private:
  void EndDragging();
  void CloseTab();

  LRESULT OnCreate(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
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
  LRESULT OnNCHitTest(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnScroll(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
  LRESULT OnSize(
      UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);

  gfx::Size initial_size_;

  // The WebContentsImpl whose contents we display.
  WebContentsImpl* web_contents_;

  content::RenderWidgetHostViewWin* view_;

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

  scoped_ptr<ui::HWNDMessageFilter> hwnd_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewWin);
};

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_
