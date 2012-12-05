// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_CONSTANTS_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_CONSTANTS_H_

namespace panel {

// Absolute minimum width and height for panels, including non-client area.
// Should only be big enough to accomodate a close button on the reasonably
// recognisable titlebar.
// These numbers are semi-arbitrary.
// Motivation for 'width' is to make main buttons on the titlebar functional.
// Motivation for height is to allow autosized tightly-wrapped panel with a
// single line of text - so the height is set to be likely less then a titlebar,
// to make sure even small content is tightly wrapped.
const int kPanelMinWidth = 80;
const int kPanelMinHeight = 20;

// The panel can be minimized to 4-pixel lines.
static const int kMinimizedPanelHeight = 4;

// The height in pixels of the titlebar.
static const int kTitlebarHeight = 40;

// The size (width or height) of the app icon (taskbar icon).
static const int kPanelAppIconSize = 32;

// Different types of buttons that can be shown on panel's titlebar.
enum TitlebarButtonType {
  CLOSE_BUTTON,
  MINIMIZE_BUTTON,
  RESTORE_BUTTON
};

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
