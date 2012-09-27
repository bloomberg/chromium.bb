// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_WIN_H_

#include "ui/views/widget/desktop_root_window_host_win.h"

class BrowserFrame;
class BrowserView;

class BrowserDesktopRootWindowHostWin : public views::DesktopRootWindowHostWin {
 public:
  BrowserDesktopRootWindowHostWin(
      views::internal::NativeWidgetDelegate* native_widget_delegate,
      const gfx::Rect& initial_bounds,
      BrowserView* browser_view,
      BrowserFrame* browser_frame);
  virtual ~BrowserDesktopRootWindowHostWin();

 private:
  // Overridden from DesktopRootWindowHostWin:
  virtual int GetInitialShowState() const OVERRIDE;
  virtual bool GetClientAreaInsets(gfx::Insets* insets) const OVERRIDE;
  virtual void HandleFrameChanged() OVERRIDE;
  virtual bool PreHandleMSG(UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT* result) OVERRIDE;
  virtual void PostHandleMSG(UINT message,
                             WPARAM w_param,
                             LPARAM l_param) OVERRIDE;
  virtual bool IsUsingCustomFrame() const OVERRIDE;

  void UpdateDWMFrame();

  BrowserView* browser_view_;
  BrowserFrame* browser_frame_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDesktopRootWindowHostWin);
};


#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_DESKTOP_ROOT_WINDOW_HOST_WIN_H_
