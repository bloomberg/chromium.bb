// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_TRANSPARENT_ACTIVATE_WINDOW_BUTTON_H_
#define ASH_WM_OVERVIEW_TRANSPARENT_ACTIVATE_WINDOW_BUTTON_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {

// Transparent window that covers window selector items and handles mouse and
// gestures on overview mode for them.
class TransparentActivateWindowButton : public views::ButtonListener {
 public:
  explicit TransparentActivateWindowButton(aura::Window* activate_window);
  virtual ~TransparentActivateWindowButton();

  // Sets the bounds of the transparent window.
  void SetBounds(const gfx::Rect& bounds);

  // Sends an a11y focus alert so that if chromevox is enabled, the window title
  // is read.
  void SendFocusAlert() const;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

 private:
  // The transparent window event handler widget itself.
  scoped_ptr<views::Widget> event_handler_widget_;

  // Pointer to the window that the button activates.
  aura::Window* activate_window_;

  DISALLOW_COPY_AND_ASSIGN(TransparentActivateWindowButton);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_TRANSPARENT_ACTIVATE_WINDOW_BUTTON_H_
