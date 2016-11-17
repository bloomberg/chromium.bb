// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/lock_window.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace chromeos {

LockWindow::LockWindow(views::View* initially_focused_view)
    : initially_focused_view_(initially_focused_view) {
  ui::GestureRecognizer::Get()->CancelActiveTouchesExcept(nullptr);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = this;
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.parent =
      ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                               ash::kShellWindowId_LockScreenContainer);
  Init(params);
  SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
}

LockWindow::~LockWindow() {}

views::Widget* LockWindow::GetWidget() {
  return this;
}

const views::Widget* LockWindow::GetWidget() const {
  return this;
}

views::View* LockWindow::GetInitiallyFocusedView() {
  return initially_focused_view_;
}

}  // namespace chromeos
