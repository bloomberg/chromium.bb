// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_STACKED_PANEL_DRAG_HANDLER_H_
#define CHROME_BROWSER_UI_PANELS_STACKED_PANEL_DRAG_HANDLER_H_

#include "base/basictypes.h"

class Panel;
namespace gfx {
class Point;
}

// Handles all the drags to unstacked free-floating panels.
class StackedPanelDragHandler {
 public:
  static void HandleDrag(Panel* panel, const gfx::Point& target_position);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(StackedPanelDragHandler);
};

#endif  // CHROME_BROWSER_UI_PANELS_STACKED_PANEL_DRAG_HANDLER_H_
