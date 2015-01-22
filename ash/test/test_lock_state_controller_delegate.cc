// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_lock_state_controller_delegate.h"

namespace ash {
namespace test {

TestLockStateControllerDelegate::TestLockStateControllerDelegate()
    : num_lock_requests_(0),
      num_shutdown_requests_(0) {
}

TestLockStateControllerDelegate::~TestLockStateControllerDelegate() {
}

void TestLockStateControllerDelegate::RequestLockScreen() {
  ++num_lock_requests_;
}

void TestLockStateControllerDelegate::RequestShutdown() {
  ++num_shutdown_requests_;
}

}  // namespace test
}  // namespace ash
