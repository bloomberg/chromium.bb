// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_CONSTANTS_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_CONSTANTS_H_

namespace panel {

// The height in pixels of the titlebar.
static const int kTitlebarHeight = 36;

// Absolute minimum width and height for panels, including non-client area.
// Should only be big enough to accomodate a close button on the reasonably
// recognisable titlebar.
// These numbers are semi-arbitrary.
// Motivation for 'width' is to make main buttons on the titlebar functional.
// Motivation for height is to allow autosized tightly-wrapped panel with a
// single line of text - so the height is set to be likely less then a titlebar,
// to make sure even small content is tightly wrapped.
const int kPanelMinWidth = 80;
const int kPanelMinHeight = kTitlebarHeight + 10;

// The panel can be minimized to 4-pixel lines.
static const int kMinimizedPanelHeight = 4;

// The size (width or height) of the app icon (taskbar icon).
static const int kPanelAppIconSize = 32;

// The size (width or height) of the button, which is also the size of the
// hit target area.
static const int kPanelButtonSize = 24;

// The padding in pixeles between the icon and the left border.
const int kTitlebarLeftPadding = 10;

// The padding in pixeles between the close button and the right border.
const int kTitlebarRightPadding = 6;

// The padding in piexles between the icon and the title text.
const int kIconAndTitlePadding = 11;

// The padding in piexles between the title text and the button.
const int kTitleAndButtonPadding = 11;

// The padding in pixeles between buttons.
static const int kButtonPadding = 5;

// The number of times to flash the panel's taskbar icon in order to draw the
// user's attention (Windows only).
static const int kNumberOfTimesToFlashPanelForAttention = 30;

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
  APPLY_TO_ALL,  // Apply the click behavior to all panels in the collection.
};

// Ways a panel can be resized.
enum Resizability {
  NOT_RESIZABLE = 0,
  RESIZABLE_TOP = 0x1,
  RESIZABLE_BOTTOM = 0x2,
  RESIZABLE_LEFT = 0x4,
  RESIZABLE_RIGHT = 0x8,
  RESIZABLE_TOP_LEFT = 0x10,
  RESIZABLE_TOP_RIGHT = 0x20,
  RESIZABLE_BOTTOM_LEFT = 0x40,
  RESIZABLE_BOTTOM_RIGHT = 0x80,
  RESIZABLE_EXCEPT_BOTTOM = RESIZABLE_TOP | RESIZABLE_LEFT | RESIZABLE_RIGHT |
      RESIZABLE_TOP_LEFT | RESIZABLE_TOP_RIGHT,
  RESIZABLE_ALL = RESIZABLE_TOP | RESIZABLE_BOTTOM | RESIZABLE_LEFT |
      RESIZABLE_RIGHT | RESIZABLE_TOP_LEFT | RESIZABLE_TOP_RIGHT |
      RESIZABLE_BOTTOM_LEFT | RESIZABLE_BOTTOM_RIGHT
};

// Describes how 4 corners of a panel should be painted.
enum CornerStyle {
  NOT_ROUNDED = 0,
  TOP_ROUNDED = 0x1,
  BOTTOM_ROUNDED = 0x2,
  ALL_ROUNDED = TOP_ROUNDED | BOTTOM_ROUNDED
};

}  // namespace panel

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_CONSTANTS_H_
