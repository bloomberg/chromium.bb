// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/native_widget_win.h"

class BrowserView;
class EncodingMenuModel;
class SystemMenuModel;
class SystemMenuModelDelegate;
class ZoomMenuModel;

namespace views {
class NativeMenuWin;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameWin
//
//  BrowserFrameWin is a NativeWidgetWin subclass that provides the window frame
//  for the Chrome browser window.
//
class BrowserFrameWin : public views::NativeWidgetWin,
                        public NativeBrowserFrame,
                        public views::ButtonListener {
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
  virtual void Show() OVERRIDE;
  virtual void ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) OVERRIDE;
  virtual void ShowWithWindowState(ui::WindowShowState show_state) OVERRIDE;
  virtual void Close() OVERRIDE;

  // Overridden from NativeBrowserFrame:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const views::NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual void InitSystemContextMenu() OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual void TabStripDisplayModeChanged() OVERRIDE;

  // Overriden from views::ImageButton override:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overridden from WindowImpl:
  virtual LRESULT OnWndProc(UINT message,
                            WPARAM w_param,
                            LPARAM l_param) OVERRIDE;

 private:
  // Updates the DWM with the frame bounds.
  void UpdateDWMFrame();

  // Builds the correct menu for when we have minimal chrome.
  void BuildSystemMenuForBrowserWindow();
  void BuildSystemMenuForAppOrPopupWindow();

  // Adds optional debug items for frame type toggling.
  void AddFrameToggleItems();

  // Handles metro navigation and search requests.
  void HandleMetroNavSearchRequest(WPARAM w_param, LPARAM l_param);

  // Returns information about the currently displayed tab in metro mode.
  void GetMetroCurrentTabInfo(WPARAM w_param);

  // Ensures that the window frame follows the Windows 8 metro app guidelines,
  // i.e. no system menu, etc.
  void AdjustFrameForImmersiveMode();

  // Called when the frame is closed. Only applies to Windows 8 metro mode.
  void CloseImmersiveFrame();

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  BrowserFrame* browser_frame_;

  // The additional items we insert into the system menu.
  scoped_ptr<SystemMenuModelDelegate> system_menu_delegate_;
  scoped_ptr<SystemMenuModel> system_menu_contents_;
  scoped_ptr<ZoomMenuModel> zoom_menu_contents_;
  scoped_ptr<EncodingMenuModel> encoding_menu_contents_;
  // The wrapped system menu itself.
  scoped_ptr<views::NativeMenuWin> system_menu_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
