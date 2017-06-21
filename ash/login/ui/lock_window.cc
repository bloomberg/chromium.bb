// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_window.h"

#include "ash/public/cpp/config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/window.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace ash {

LockWindow::LockWindow() {
  ui::GestureRecognizer::Get()->CancelActiveTouchesExcept(nullptr);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = this;
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  const int kLockContainer = ash::kShellWindowId_LockScreenContainer;
  if (Shell::GetAshConfig() == Config::MASH) {
    params.mus_properties[ui::mojom::WindowManager::kContainerId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(kLockContainer);
  } else {
    params.parent = ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                             kLockContainer);
  }
  Init(params);
  SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
}

LockWindow::~LockWindow() {
  // We need to destroy the root view before destroying |data_dispatcher_|
  // because lock screen destruction assumes it is alive. We could hand out
  // base::WeakPtr<LoginDataDispatcher> instances if needed instead.
  delete views::Widget::GetContentsView();
}

views::Widget* LockWindow::GetWidget() {
  return this;
}

const views::Widget* LockWindow::GetWidget() const {
  return this;
}

views::View* LockWindow::GetInitiallyFocusedView() {
  // There are multiple GetContentsView definitions; use the views::Widget one.
  return views::Widget::GetContentsView();
}

}  // namespace ash
