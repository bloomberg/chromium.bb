// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/minimize_button_metrics_win.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/native_widget_win.h"

class BrowserView;

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
  virtual void OnScreenReaderDetected() OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) OVERRIDE;
  virtual void ShowWithWindowState(ui::WindowShowState show_state) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void FrameTypeChanged() OVERRIDE;
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual void Activate() OVERRIDE;

  // Overridden from NativeBrowserFrame:
  virtual views::NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const views::NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual bool UsesNativeSystemMenu() const OVERRIDE;
  virtual int GetMinimizeButtonOffset() const OVERRIDE;
  virtual void TabStripDisplayModeChanged() OVERRIDE;

  // Overriden from views::ImageButton override:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  // Updates the DWM with the frame bounds.
  void UpdateDWMFrame();

  // Handles metro navigation and search requests.
  void HandleMetroNavSearchRequest(WPARAM w_param, LPARAM l_param);

  // Returns information about the currently displayed tab in metro mode.
  void GetMetroCurrentTabInfo(WPARAM w_param);

  // Ensures that the window frame follows the Windows 8 metro app guidelines,
  // i.e. no system menu, etc.
  void AdjustFrameForImmersiveMode();

  // Called when the frame is closed. Only applies to Windows 8 metro mode.
  void CloseImmersiveFrame();

  views::NativeMenuWin* GetSystemMenu();

  // The BrowserView is our ClientView. This is a pointer to it.
  BrowserView* browser_view_;

  BrowserFrame* browser_frame_;

  // The wrapped system menu itself.
  scoped_ptr<views::NativeMenuWin> system_menu_;

  MinimizeButtonMetrics minimize_button_metrics_;

  DISALLOW_COPY_AND_ASSIGN(BrowserFrameWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_WIN_H_
