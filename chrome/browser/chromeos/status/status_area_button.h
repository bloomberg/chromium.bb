// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_BUTTON_H_
#pragma once

#include "views/controls/button/menu_button.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace chromeos {

// Button to be used to represent status and allow menus to be popped up.
// Shows current button state by drawing a border around the current icon.
class StatusAreaButton : public views::MenuButton {
 public:
  explicit StatusAreaButton(views::ViewMenuDelegate* menu_delegate);
  virtual ~StatusAreaButton() {}
  virtual void Paint(gfx::Canvas* canvas, bool for_drag);
  virtual gfx::Size GetPreferredSize();
  virtual gfx::Insets GetInsets() const;

  // Overrides TextButton's SetText to clear max text size before seting new
  // text content so that the button size would fit the new text size.
  virtual void SetText(const std::wstring& text);

  void set_use_menu_button_paint(bool use_menu_button_paint) {
    use_menu_button_paint_ = use_menu_button_paint;
  }

  // views::MenuButton overrides.
  virtual bool Activate();

  // Controls whether or not this status area button is able to be pressed.
  void set_active(bool active) { active_ = active; }
  bool active() const { return active_; }

 protected:

  // Draws the icon for this status area button on the canvas.
  // Subclasses should override this method if they need to draw their own icon.
  // Otherwise, just call SetIcon() and the it will be handled for you.
  virtual void DrawIcon(gfx::Canvas* canvas);

  // Subclasses should override these methods to return the correct dimensions.
  virtual int icon_height() { return 24; }
  virtual int icon_width() { return 23; }

  // Subclasses can override this method to return more or less padding.
  // The padding is added to both the left and right side.
  virtual int horizontal_padding() { return 1; }

  // True if the button wants to use views::MenuButton drawings.
  bool use_menu_button_paint_;

  // Insets to use for this button.
  gfx::Insets insets_;

  // Indicates when this button can be pressed.  Independent of
  // IsEnabled state, so that when IsEnabled is true, this can still
  // be false, and vice versa.
  bool active_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_STATUS_AREA_BUTTON_H_
