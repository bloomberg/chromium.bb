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
#include "content/common/drag_event_source_info.h"
#include "content/port/browser/render_view_host_delegate_view.h"
#include "content/port/browser/web_contents_view_port.h"
#include "ui/base/win/window_impl.h"

namespace ui {
class HWNDMessageFilter;
}

namespace content {
class WebContentsDragWin;
class WebContentsImpl;
class WebContentsViewDelegate;
class WebDragDest;

// An implementation of WebContentsView for Windows.
class CONTENT_EXPORT WebContentsViewWin
    : public WebContentsViewPort,
      public RenderViewHostDelegateView,
      public ui::WindowImpl {
 public:
  WebContentsViewWin(WebContentsImpl* web_contents,
                     WebContentsViewDelegate* delegate);
  virtual ~WebContentsViewWin();

  BEGIN_MSG_MAP_EX(WebContentsViewWin)
    MESSAGE_HANDLER(WM_CREATE, OnCreate)
    MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
    MESSAGE_HANDLER(WM_WINDOWPOSCHANGED, OnWindowPosChanged)
    MESSAGE_HANDLER(WM_LBUTTONDOWN, OnMouseDown)
    MESSAGE_HANDLER(WM_MBUTTONDOWN, OnMouseDown)
    MESSAGE_HANDLER(WM_RBUTTONDOWN, OnMouseDown)
    MESSAGE_HANDLER(WM_MOUSEMOVE, OnMouseMove)
    // Hacks for old ThinkPad touchpads/scroll points.
    MESSAGE_HANDLER(WM_NCCALCSIZE, OnNCCalcSize)
    MESSAGE_HANDLER(WM_NCHITTEST, OnNCHitTest)
    MESSAGE_HANDLER(WM_HSCROLL, OnScroll)
    MESSAGE_HANDLER(WM_VSCROLL, OnScroll)
    MESSAGE_HANDLER(WM_SIZE, OnSize)
  END_MSG_MAP()

  // Overridden from WebContentsView:
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeView GetContentNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void GetContainerBounds(gfx::Rect *out) const OVERRIDE;
  virtual void OnTabCrashed(base::TerminationStatus status,
                            int error_code) OVERRIDE;
  virtual void SizeContents(const gfx::Size& size) OVERRIDE;
  virtual void Focus() OVERRIDE;
  virtual void SetInitialFocus() OVERRIDE;
  virtual void StoreFocus() OVERRIDE;
  virtual void RestoreFocus() OVERRIDE;
  virtual WebDropData* GetDropData() const OVERRIDE;
  virtual gfx::Rect GetViewBounds() const OVERRIDE;

  // Overridden from WebContentsViewPort:
  virtual void CreateView(
      const gfx::Size& initial_size, gfx::NativeView context) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual RenderWidgetHostView* CreateViewForPopupWidget(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void RenderViewCreated(RenderViewHost* host) OVERRIDE;
  virtual void RenderViewSwappedIn(RenderViewHost* host) OVERRIDE;

  // Implementation of RenderViewHostDelegateView.
  virtual void ShowContextMenu(
      const ContextMenuParams& params,
      ContextMenuSourceType type) OVERRIDE;
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
                             const gfx::Vector2d& image_offset,
                             const DragEventSourceInfo& event_info) OVERRIDE;
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

  scoped_ptr<WebContentsViewDelegate> delegate_;

  // The helper object that handles drag destination related interactions with
  // Windows.
  scoped_refptr<WebDragDest> drag_dest_;

  // Used to handle the drag-and-drop.
  scoped_refptr<WebContentsDragWin> drag_handler_;

  scoped_ptr<ui::HWNDMessageFilter> hwnd_message_filter_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsViewWin);
};

} // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_VIEW_WIN_H_
