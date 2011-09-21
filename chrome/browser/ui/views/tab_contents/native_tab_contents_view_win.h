// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_WIN_H_
#pragma once

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view.h"
#include "views/widget/native_widget_win.h"

class WebDropTarget;
class TabContents;
class TabContentsDragWin;

class NativeTabContentsViewWin : public views::NativeWidgetWin,
                                 public NativeTabContentsView {
 public:
  explicit NativeTabContentsViewWin(
      internal::NativeTabContentsViewDelegate* delegate);
  virtual ~NativeTabContentsViewWin();

  WebDropTarget* drop_target() const { return drop_target_.get(); }

  TabContents* GetTabContents() const;

  void EndDragging();

 private:
  // Overridden from NativeTabContentsView:
  virtual void InitNativeTabContentsView() OVERRIDE;
  virtual void Unparent() OVERRIDE;
  virtual RenderWidgetHostView* CreateRenderWidgetHostView(
      RenderWidgetHost* render_widget_host) OVERRIDE;
  virtual gfx::NativeWindow GetTopLevelNativeWindow() const OVERRIDE;
  virtual void SetPageTitle(const string16& title) OVERRIDE;
  virtual void StartDragging(const WebDropData& drop_data,
                             WebKit::WebDragOperationsMask ops,
                             const SkBitmap& image,
                             const gfx::Point& image_offset) OVERRIDE;
  virtual void CancelDrag() OVERRIDE;
  virtual bool IsDoingDrag() const OVERRIDE;
  virtual void SetDragCursor(WebKit::WebDragOperation operation) OVERRIDE;
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;

  // Overridden from views::NativeWidgetWin:
  virtual void OnDestroy() OVERRIDE;
  virtual void OnHScroll(int scroll_type,
                         short position,
                         HWND scrollbar) OVERRIDE;
  virtual LRESULT OnMouseRange(UINT msg,
                               WPARAM w_param,
                               LPARAM l_param) OVERRIDE;
  virtual LRESULT OnReflectedMessage(UINT msg,
                                     WPARAM w_param,
                                     LPARAM l_param) OVERRIDE;
  virtual void OnVScroll(int scroll_type,
                         short position,
                         HWND scrollbar) OVERRIDE;
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos) OVERRIDE;
  virtual void OnSize(UINT param, const WTL::CSize& size) OVERRIDE;
  virtual LRESULT OnNCCalcSize(BOOL w_param, LPARAM l_param) OVERRIDE;
  virtual void OnNCPaint(HRGN rgn) OVERRIDE;

  // Backend for all scroll messages, the |message| parameter indicates which
  // one it is.
  void ScrollCommon(UINT message, int scroll_type, short position,
                    HWND scrollbar);
  bool ScrollZoom(int scroll_type);

  internal::NativeTabContentsViewDelegate* delegate_;

  // A drop target object that handles drags over this TabContents.
  scoped_refptr<WebDropTarget> drop_target_;

  // Used to handle the drag-and-drop.
  scoped_refptr<TabContentsDragWin> drag_handler_;

  DISALLOW_COPY_AND_ASSIGN(NativeTabContentsViewWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_CONTENTS_NATIVE_TAB_CONTENTS_VIEW_WIN_H_
