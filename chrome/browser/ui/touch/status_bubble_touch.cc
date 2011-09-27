// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/status_bubble_touch.h"

#include "chrome/browser/ui/virtual_keyboard/virtual_keyboard_manager.h"

StatusBubbleTouch::StatusBubbleTouch(views::View* base_view)
    : StatusBubbleViews(base_view) {
  VirtualKeyboardManager::GetInstance()->keyboard()->AddObserver(this);
}

StatusBubbleTouch::~StatusBubbleTouch() {
  views::Widget* keyboard = VirtualKeyboardManager::GetInstance()->keyboard();
  if (keyboard)
    keyboard->RemoveObserver(this);
}

void StatusBubbleTouch::Reposition() {
  StatusBubbleViews::Reposition();
  views::Widget* keyboard = VirtualKeyboardManager::GetInstance()->keyboard();
  if (popup() && keyboard && keyboard->IsVisible()) {
    gfx::Rect popup_screen = popup()->GetWindowScreenBounds();
    gfx::Rect keyboard_screen = keyboard->GetWindowScreenBounds();
    if (popup_screen.Intersects(keyboard_screen)) {
      // The keyboard may be hiding the popup. Reposition above the keyboard.
      popup_screen.set_y(keyboard_screen.y() - popup_screen.height());
      popup()->SetBounds(popup_screen);
    }
  }
}

void StatusBubbleTouch::OnWidgetVisibilityChanged(views::Widget* widget,
                                                  bool visible) {
  Reposition();
}
