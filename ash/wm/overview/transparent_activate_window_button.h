// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_TRANSPARENT_ACTIVATE_WINDOW_BUTTON_H_
#define ASH_WM_OVERVIEW_TRANSPARENT_ACTIVATE_WINDOW_BUTTON_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Button;
class Widget;
}  // namespace views

namespace ash {

class TransparentActivateWindowButtonDelegate;

// Transparent window that covers window selector items and handles mouse and
// gestures on overview mode for them.
class TransparentActivateWindowButton {
 public:
  TransparentActivateWindowButton(
      aura::Window* root_window,
      TransparentActivateWindowButtonDelegate* delegate);
  ~TransparentActivateWindowButton();

  // Sets the accessibility name.
  void SetAccessibleName(const base::string16& name);

  // Sets the bounds of the transparent window.
  void SetBounds(const gfx::Rect& bounds);

  // Sends an accessibility focus alert so that if chromevox is enabled, the
  // window title is read.
  void SendFocusAlert() const;

  // Stacks |this| above |activate_button| in the window z-ordering.
  void StackAbove(TransparentActivateWindowButton* activate_button);

 private:
  // The transparent window event handler widget itself.
  scoped_ptr<views::Widget> event_handler_widget_;

  // The transparent button view that handles user input. Not owned.
  views::Button* transparent_button_;

  DISALLOW_COPY_AND_ASSIGN(TransparentActivateWindowButton);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_TRANSPARENT_ACTIVATE_WINDOW_BUTTON_H_
