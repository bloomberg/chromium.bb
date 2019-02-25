// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/test_condition_waiter.h"

#include "base/callback.h"

namespace chromeos {
namespace test {
namespace {

const base::TimeDelta kConditionCheckFrequency =
    base::TimeDelta::FromMilliseconds(200);

}  // anonymous namespace

TestConditionWaiter::TestConditionWaiter(
    const base::RepeatingCallback<bool(void)>& is_fulfilled)
    : is_fulfilled_(is_fulfilled) {}

TestConditionWaiter::~TestConditionWaiter() = default;

void TestConditionWaiter::Wait() {
  if (is_fulfilled_.Run())
    return;

  timer_.Start(FROM_HERE, kConditionCheckFrequency, this,
               &TestConditionWaiter::CheckCondition);
  run_loop_.Run();
}

void TestConditionWaiter::CheckCondition() {
  if (is_fulfilled_.Run()) {
    run_loop_.Quit();
    timer_.Stop();
  }
}

}  // namespace test
}  // namespace chromeos
