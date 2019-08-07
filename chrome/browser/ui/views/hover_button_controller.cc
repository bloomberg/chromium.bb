// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hover_button_controller.h"

#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/button/button_controller_delegate.h"
#include "ui/views/mouse_constants.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

HoverButtonController::HoverButtonController(
    views::Button* button,
    views::ButtonListener* listener,
    std::unique_ptr<views::ButtonControllerDelegate> delegate)
    : ButtonController(button, std::move(delegate)), listener_(listener) {
  set_notify_action(views::ButtonController::NotifyAction::NOTIFY_ON_RELEASE);
}

HoverButtonController::~HoverButtonController() = default;

bool HoverButtonController::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN && listener_) {
    listener_->ButtonPressed(button(), event);
    return true;
  }
  return false;
}

bool HoverButtonController::OnMousePressed(const ui::MouseEvent& event) {
  DCHECK(notify_action() ==
         views::ButtonController::NotifyAction::NOTIFY_ON_RELEASE);
  if (button()->request_focus_on_press())
    button()->RequestFocus();
  if (listener_) {
    button()->AnimateInkDrop(views::InkDropState::ACTION_TRIGGERED,
                             ui::LocatedEvent::FromIfValid(&event));
  } else {
    button()->AnimateInkDrop(views::InkDropState::HIDDEN,
                             ui::LocatedEvent::FromIfValid(&event));
  }
  return true;
}

void HoverButtonController::OnMouseReleased(const ui::MouseEvent& event) {
  DCHECK(notify_action() ==
         views::ButtonController::NotifyAction::NOTIFY_ON_RELEASE);
  if (button()->state() != views::Button::STATE_DISABLED) {
    if (listener_)
      listener_->ButtonPressed(button(), event);
  } else {
    button()->AnimateInkDrop(views::InkDropState::HIDDEN, &event);
    ButtonController::OnMouseReleased(event);
  }
}
