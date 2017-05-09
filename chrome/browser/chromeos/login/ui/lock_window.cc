// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/lock_window.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/window.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace chromeos {

LockWindow::LockWindow() {
  ui::GestureRecognizer::Get()->CancelActiveTouchesExcept(nullptr);

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = this;
  params.show_state = ui::SHOW_STATE_FULLSCREEN;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  const int kLockContainer = ash::kShellWindowId_LockScreenContainer;
  if (ash_util::IsRunningInMash()) {
    using ui::mojom::WindowManager;
    params.mus_properties[WindowManager::kContainerId_InitProperty] =
        mojo::ConvertTo<std::vector<uint8_t>>(kLockContainer);
  } else {
    params.parent = ash::Shell::GetContainer(ash::Shell::GetPrimaryRootWindow(),
                                             kLockContainer);
  }
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
  // There are multiple GetContentsView definitions; use the views::Widget one.
  return views::Widget::GetContentsView();
}

}  // namespace chromeos
