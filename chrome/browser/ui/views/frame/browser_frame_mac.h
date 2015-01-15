// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_MAC_H_

#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "ui/views/widget/native_widget_mac.h"

class BrowserFrame;
class BrowserView;

////////////////////////////////////////////////////////////////////////////////
//  BrowserFrameMac is a NativeWidgetMac subclass that provides
//  the window frame for the Chrome browser window.
//
class BrowserFrameMac : public views::NativeWidgetMac,
                        public NativeBrowserFrame {
 public:
  BrowserFrameMac(BrowserFrame* browser_frame, BrowserView* browser_view);

  // Overridden from NativeBrowserFrame:
  views::Widget::InitParams GetWidgetParams() override;
  bool UseCustomFrame() const override;
  bool UsesNativeSystemMenu() const override;
  bool ShouldSaveWindowPlacement() const override;
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const override;

 protected:
  ~BrowserFrameMac() override;

  // Overridden from NativeBrowserFrame:
  int GetMinimizeButtonOffset() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserFrameMac);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_MAC_H_
