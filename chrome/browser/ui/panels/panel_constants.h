// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_CONSTANTS_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_CONSTANTS_H_
#pragma once

namespace panel {

  // Different platforms use different modifier keys to change the behavior
  // of a mouse click. This enum captures the meaning of the modifier rather
  // than the actual modifier key to generalize across platforms.
  enum ClickModifier {
    NO_MODIFIER,
    APPLY_TO_ALL,  // Apply the click behavior to all panels in the strip.
  };

  // Edge at which a panel is being resized using the mouse.
  enum ResizingSides {
    RESIZE_NONE = 0,
    RESIZE_TOP,
    RESIZE_TOP_RIGHT,
    RESIZE_RIGHT,
    RESIZE_BOTTOM_RIGHT,
    RESIZE_BOTTOM,
    RESIZE_BOTTOM_LEFT,
    RESIZE_LEFT,
    RESIZE_TOP_LEFT
  };

  // Ways a panel can be resized.
  enum Resizability {
    NOT_RESIZABLE,
    RESIZABLE_ALL_SIDES,
    RESIZABLE_ALL_SIDES_EXCEPT_BOTTOM
  };

}  // namespace panel

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_CONSTANTS_H_
