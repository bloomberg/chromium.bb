// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/power/native_timer.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// Returns true iff |Start| on |timer| succeeds and timer expiration occurs too.
// The underlying fake power manager implementation expires the timer
// immediately and the test doesn't sleep.
bool CheckStartTimerAndExpiration(NativeTimer* timer,
                                  base::TimeTicks absolute_expiration_ticks) {
  base::RunLoop start_timer_loop;
  base::RunLoop expiration_loop;
  bool start_timer_result = false;
  bool expiration_result = false;
  timer->Start(
      absolute_expiration_ticks,
      base::BindOnce(
          [](bool* result_out, base::OnceClosure quit_closure) {
            *result_out = true;
            std::move(quit_closure).Run();
          },
          &expiration_result, expiration_loop.QuitClosure()),
      base::BindOnce(
          [](bool* result_out, base::OnceClosure quit_closure, bool result) {
            *result_out = result;
            std::move(quit_closure).Run();
          },
          &start_timer_result, start_timer_loop.QuitClosure()));
  start_timer_loop.Run();
  if (!start_timer_result) {
    return false;
  }

  // Run until timer expiration and check for the result.
  expiration_loop.Run();
  if (!expiration_result) {
    return false;
  }
  return true;
}

}  // namespace

class NativeTimerTest : public testing::Test {
 public:
  NativeTimerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

  ~NativeTimerTest() override = default;

  // testing::Test:
  void SetUp() override { PowerManagerClient::Initialize(); }

  void TearDown() override { PowerManagerClient::Shutdown(); }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeTimerTest);
};

TEST_F(NativeTimerTest, CheckCreateFailure) {
  // Create the timer. It queues async operations; enclose it in a run loop.
  // This should fail internally as an empty tag is provided.
  base::RunLoop create_timer_loop;
  NativeTimer timer("");
  create_timer_loop.RunUntilIdle();

  // Starting the timer should fail as timer creation failed.
  EXPECT_FALSE(CheckStartTimerAndExpiration(
      &timer, base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000)));
}

TEST_F(NativeTimerTest, CheckCreateAndStartTimer) {
  // Create the timer. It queues async operations; enclose it in a run loop.
  base::RunLoop create_timer_loop;
  NativeTimer timer("Assistant");
  create_timer_loop.RunUntilIdle();

  // Start timer and check if starting the timer and its expiration succeeded.
  EXPECT_TRUE(CheckStartTimerAndExpiration(
      &timer, base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000)));

  // Start another timer and check if starting the timer and its expiration
  // succeeded.
  EXPECT_TRUE(CheckStartTimerAndExpiration(
      &timer, base::TimeTicks() + base::TimeDelta::FromMilliseconds(1000)));
}

}  // namespace chromeos
