// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_BASE_TAB_STRIP_H_
#define CHROME_BROWSER_VIEWS_TABS_BASE_TAB_STRIP_H_

#include "views/view.h"

class TabStrip;

// A class that represents the common interface to the tabstrip used by classes
// such as BrowserView etc.
class BaseTabStrip : public views::View {
 public:
  // Returns the preferred height of this TabStrip. This is based on the
  // typical height of its constituent tabs.
  virtual int GetPreferredHeight() = 0;

  // Set the background offset used by inactive tabs to match the frame image.
  virtual void SetBackgroundOffset(const gfx::Point& offset) = 0;

  // Returns true if the specified point(TabStrip coordinates) is
  // in the window caption area of the browser window.
  virtual bool IsPositionInWindowCaption(const gfx::Point& point) = 0;

  // Sets the bounds of the tab at the specified |tab_index|. |tab_bounds| are
  // in TabStrip coordinates.
  virtual void SetDraggedTabBounds(int tab_index,
                                   const gfx::Rect& tab_bounds) = 0;

  // Returns true if a drag session is currently active.
  virtual bool IsDragSessionActive() const = 0;

  // Updates the loading animations displayed by tabs in the tabstrip to the
  // next frame.
  virtual void UpdateLoadingAnimations() = 0;

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  virtual bool IsAnimating() const = 0;

  // Returns this object as a TabStrip if it is one.
  virtual TabStrip* AsTabStrip() = 0;
};

#endif  // CHROME_BROWSER_VIEWS_TABS_BASE_TAB_STRIP_H_

