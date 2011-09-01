// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_STRIP_H_
#define CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_STRIP_H_
#pragma once

#include "chrome/browser/ui/views/tabs/tab_strip.h"

namespace ui {
enum TouchStatus;
}

class TabStripSelectionModel;
class TouchTab;

///////////////////////////////////////////////////////////////////////////////
//
// TouchTabStrip
//
//  A View that overrides Touch specific behavior.
//
///////////////////////////////////////////////////////////////////////////////
class TouchTabStrip : public TabStrip {
 public:
  explicit TouchTabStrip(TabStripController* controller);
  virtual ~TouchTabStrip();

  // views::View overrides:
  virtual void OnDragEntered(const views::DropTargetEvent& event) OVERRIDE;
  virtual int OnDragUpdated(const views::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragExited() OVERRIDE;
  virtual int OnPerformDrop(const views::DropTargetEvent& event) OVERRIDE;

 protected:
  // BaseTabStrip overrides:
  virtual BaseTab* CreateTab() OVERRIDE;

 private:
  virtual ui::TouchStatus OnTouchEvent(const views::TouchEvent& event) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TouchTabStrip);
};

#endif  // CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_STRIP_H_
