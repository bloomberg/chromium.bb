// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/focus_manager_factory.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

AshFocusManagerFactory::AshFocusManagerFactory() {}
AshFocusManagerFactory::~AshFocusManagerFactory() {}

views::FocusManager* AshFocusManagerFactory::CreateFocusManager(
    views::Widget* widget,
    bool desktop_widget) {
  return new views::FocusManager(widget, desktop_widget ? NULL : new Delegate);
}

bool AshFocusManagerFactory::Delegate::ProcessAccelerator(
    const ui::Accelerator& accelerator) {
  AcceleratorController* controller =
      Shell::GetInstance()->accelerator_controller();
  if (controller)
    return controller->Process(accelerator);
  return false;
}

}  // namespace ash
