// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/accelerators/ash_focus_manager_factory.h"

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/wm_shell.h"
#include "base/memory/ptr_util.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

AshFocusManagerFactory::AshFocusManagerFactory() {}
AshFocusManagerFactory::~AshFocusManagerFactory() {}

views::FocusManager* AshFocusManagerFactory::CreateFocusManager(
    views::Widget* widget,
    bool desktop_widget) {
  return new views::FocusManager(
      widget,
      desktop_widget ? nullptr : base::WrapUnique<Delegate>(new Delegate));
}

bool AshFocusManagerFactory::Delegate::ProcessAccelerator(
    const ui::Accelerator& accelerator) {
  AcceleratorController* controller = WmShell::Get()->accelerator_controller();
  if (controller)
    return controller->Process(accelerator);
  return false;
}

}  // namespace ash
