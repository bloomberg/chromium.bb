// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "views/widget/native_widget_win.h"

class BrowserView;

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin
//
//  BrowserFrameWin is a NativeWidgetWin subclass that provides the window frame
//  for the Chrome browser window.
//
class BrowserFrameWin : public views::NativeWidgetWin,
                        public NativeBrowserFrame {
 public:
  BrowserFrameWin(BrowserFrame* browser_frame, BrowserView* browser_view);
  virtual ~BrowserFrameWin();

  BrowserView* browser_view() const { return browser_view_; }

  // Explicitly sets how windows are shown. Use a value of -1 to give the
  // default behavior. This is used during testing and not generally useful
  // otherwise.
  static void SetShowState(int state);

 protected:
  // Overridden from views::NativeWidgetWin:
  virtual int GetShowState() const OVERRIDE;
  virtual gfx::Insets GetClientAreaInsets() const OVERRIDE;
  virtual void UpdateFrameAfterFrameChange() OVERRIDE;
  virtual void OnEndSession(BOOL ending, UINT logoff) OVERRIDE;
  virtual void OnInitMenuPopup(HMENU menu,
                               UINT position,
                               BOOL is_system_menu) OVERRIDE;
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos) OVERRIDE;
  virtual void OnScreenReaderDetected() OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;

  // Overridden from NativeBrowserFrame:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const views::NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual void TabStripDisplayModeChanged() OVERRIDE;

 private:
  // Updates the DWM with the frame bounds.
  void UpdateDWMFrame();

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  BrowserFrame* browser_frame_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
