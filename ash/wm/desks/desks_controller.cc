// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_controller.h"

#include "ash/shell.h"
#include "base/logging.h"

namespace ash {

// static
DesksController* DesksController::Get() {
  return Shell::Get()->desks_controller();
}

bool DesksController::CanCreateDesks() const {
  // TODO(afakhry): Maximum of four desks.
  return true;
}

void DesksController::NewDesk() {
  NOTIMPLEMENTED();
}

}  // namespace ash
