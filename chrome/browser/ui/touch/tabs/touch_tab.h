// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_H_
#define CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_H_
#pragma once

#include "chrome/browser/ui/views/tabs/tab.h"

///////////////////////////////////////////////////////////////////////////////
//
// TouchTab
//
//  A View that renders a TouchTab in a TouchTabStrip
//
///////////////////////////////////////////////////////////////////////////////
class TouchTab : public Tab {
 public:
  // The size of the favicon touch area.
  static const int kTouchTargetIconSize = 32;

  explicit TouchTab(TabController* controller);
  virtual ~TouchTab();

  // get and set touch icon
  void set_touch_icon(const SkBitmap& bitmap) {
    touch_icon_ = bitmap;
  }
  const SkBitmap& touch_icon() const {
    return touch_icon_;
  }

 protected:
  // Returns whether the Tab should display a close button.
  virtual bool ShouldShowCloseBox() const OVERRIDE;

 private:
  // The touch icon found
  SkBitmap touch_icon_;

  DISALLOW_COPY_AND_ASSIGN(TouchTab);
};

#endif  // CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_H_
