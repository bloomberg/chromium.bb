// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/lock_state_controller_test_api.h"

#include <memory>
#include <utility>

#include "ash/public/interfaces/shutdown.mojom.h"

namespace ash {
namespace test {

LockStateControllerTestApi::LockStateControllerTestApi(
    LockStateController* controller)
    : controller_(controller) {
  DCHECK(controller);
}

LockStateControllerTestApi::~LockStateControllerTestApi() {}

}  // namespace test
}  // namespace ash
