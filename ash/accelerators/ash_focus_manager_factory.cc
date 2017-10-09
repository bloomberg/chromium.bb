// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/ash_focus_manager_factory.h"

#include <memory>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

AshFocusManagerFactory::AshFocusManagerFactory() {}
AshFocusManagerFactory::~AshFocusManagerFactory() {}

std::unique_ptr<views::FocusManager> AshFocusManagerFactory::CreateFocusManager(
    views::Widget* widget,
    bool desktop_widget) {
  return std::make_unique<views::FocusManager>(
      widget, desktop_widget ? nullptr : std::make_unique<Delegate>());
}

AshFocusManagerFactory::Delegate::Delegate() {}
AshFocusManagerFactory::Delegate::~Delegate() {}

bool AshFocusManagerFactory::Delegate::ProcessAccelerator(
    const ui::Accelerator& accelerator) {
  AcceleratorController* controller = Shell::Get()->accelerator_controller();
  return controller && controller->Process(accelerator);
}

}  // namespace ash
