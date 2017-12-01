// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller_delegate_mash.h"

#include "base/logging.h"

namespace ash {

AcceleratorControllerDelegateMash::AcceleratorControllerDelegateMash() =
    default;

AcceleratorControllerDelegateMash::~AcceleratorControllerDelegateMash() =
    default;

bool AcceleratorControllerDelegateMash::HandlesAction(
    AcceleratorAction action) {
  return false;
}

bool AcceleratorControllerDelegateMash::CanPerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator,
    const ui::Accelerator& previous_accelerator) {
  return false;
}

void AcceleratorControllerDelegateMash::PerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {}

}  // namespace ash
