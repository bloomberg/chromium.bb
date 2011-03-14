// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "views/window/window_win.h"

class BrowserView;
class Profile;

///////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin
//
//  BrowserFrame is a WindowWin subclass that provides the window frame for the
//  Chrome browser window.
//
class BrowserFrameWin : public BrowserFrame,
                        public views::WindowWin,
                        public NativeBrowserFrame {
 public:
  // Normally you will create this class by calling BrowserFrame::Create.
  // Init must be called before using this class, which Create will do for you.
  BrowserFrameWin(BrowserView* browser_view, Profile* profile);
  virtual ~BrowserFrameWin();

  // This initialization function must be called after construction, it is
  // separate to avoid recursive calling of the frame from its constructor.
  void InitBrowserFrame();

  BrowserView* browser_view() const { return browser_view_; }

  // Explicitly sets how windows are shown. Use a value of -1 to give the
  // default behavior. This is used during testing and not generally useful
  // otherwise.
  static void SetShowState(int state);

 protected:
  // Overridden from views::WindowWin:
  virtual int GetShowState() const OVERRIDE;
  virtual gfx::Insets GetClientAreaInsets() const OVERRIDE;
  virtual bool GetAccelerator(int cmd_id,
                              ui::Accelerator* accelerator) OVERRIDE;
  virtual void OnEndSession(BOOL ending, UINT logoff) OVERRIDE;
  virtual void OnInitMenuPopup(HMENU menu,
                               UINT position,
                               BOOL is_system_menu) OVERRIDE;
  virtual void OnWindowPosChanged(WINDOWPOS* window_pos) OVERRIDE;
  virtual ui::ThemeProvider* GetThemeProvider() const OVERRIDE;
  virtual void OnScreenReaderDetected() OVERRIDE;

  // Overridden from views::Window:
  virtual void Activate() OVERRIDE;
  virtual bool IsAppWindow() const OVERRIDE { return true; }
  virtual void UpdateFrameAfterFrameChange() OVERRIDE;
  virtual views::RootView* CreateRootView() OVERRIDE;
  virtual views::NonClientFrameView* CreateFrameViewForWindow() OVERRIDE;

  // Overridden from NativeBrowserFrame:
  virtual views::NativeWindow* AsNativeWindow() OVERRIDE;
  virtual const views::NativeWindow* AsNativeWindow() const OVERRIDE;
  virtual BrowserNonClientFrameView* CreateBrowserNonClientFrameView() OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual ui::ThemeProvider* GetThemeProviderForFrame() const OVERRIDE;
  virtual bool AlwaysUseNativeFrame() const OVERRIDE;
  virtual void TabStripDisplayModeChanged() OVERRIDE;

 private:
  // Updates the DWM with the frame bounds.
  void UpdateDWMFrame();

  NativeBrowserFrameDelegate* delegate_;

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
