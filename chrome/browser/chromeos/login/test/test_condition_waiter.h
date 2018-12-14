// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_TEST_CONDITION_WAITER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_TEST_CONDITION_WAITER_H_

#include "base/callback_forward.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"

namespace chromeos {
namespace test {

// Waits for condition to be fulfilled.
class TestConditionWaiter {
 public:
  using ConditionCheck = base::RepeatingCallback<bool(void)>;

  explicit TestConditionWaiter(const ConditionCheck& is_fulfilled);
  ~TestConditionWaiter();

  void Wait();

 private:
  void CheckCondition();

  const ConditionCheck is_fulfilled_;

  base::RepeatingTimer timer_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestConditionWaiter);
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_TEST_CONDITION_WAITER_H_
