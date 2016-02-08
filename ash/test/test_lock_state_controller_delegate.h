// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_LOCK_STATE_CONTROLLER_DELEGATE_H_
#define ASH_TEST_TEST_LOCK_STATE_CONTROLLER_DELEGATE_H_

#include "ash/wm/lock_state_controller.h"
#include "base/macros.h"

namespace ash {
namespace test {

// Fake implementation of PowerButtonControllerDelegate that just logs requests
// to lock the screen and shut down the device.
class TestLockStateControllerDelegate : public LockStateControllerDelegate {
 public:
  TestLockStateControllerDelegate();
  ~TestLockStateControllerDelegate() override;

  int num_lock_requests() const { return num_lock_requests_; }
  int num_shutdown_requests() const { return num_shutdown_requests_; }

  // LockStateControllerDelegate implementation.
  bool IsLoading() const override;
  void RequestLockScreen() override;
  void RequestShutdown() override;

 private:
  int num_lock_requests_;
  int num_shutdown_requests_;

  DISALLOW_COPY_AND_ASSIGN(TestLockStateControllerDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_LOCK_STATE_CONTROLLER_DELEGATE_H_
