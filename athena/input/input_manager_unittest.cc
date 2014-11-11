// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/input/input_manager_impl.h"

#include "athena/input/public/accelerator_manager.h"
#include "athena/test/base/athena_test_base.h"
#include "athena/util/switches.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "ui/events/test/event_generator.h"

namespace athena {
namespace {

class TestPowerButtonObserver : public PowerButtonObserver {
 public:
  TestPowerButtonObserver() : count_(0), state_(RELEASED) {
    InputManager::Get()->AddPowerButtonObserver(this);
  }
  ~TestPowerButtonObserver() override {
    InputManager::Get()->RemovePowerButtonObserver(this);
  }

  int count() const { return count_; }
  State state() const { return state_; }

  bool WaitForLongPress() {
    run_loop_.Run();
    return state_ == LONG_PRESSED;
  }

 private:
  virtual void OnPowerButtonStateChanged(
      PowerButtonObserver::State state) override {
    state_ = state;
    count_++;
    if (state == LONG_PRESSED) {
      DCHECK(run_loop_.running());
      run_loop_.Quit();
    }
  }
  base::RunLoop run_loop_;
  int count_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(TestPowerButtonObserver);
};

class InputManagerTest : public test::AthenaTestBase {
 public:
  InputManagerTest() {}
  ~InputManagerTest() override {}

  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kEnableDebugAccelerators);
    test::AthenaTestBase::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InputManagerTest);
};

}  // namespace

namespace test {

class ScopedPowerButtonTimeoutShortener {
 public:
  ScopedPowerButtonTimeoutShortener()
      : original_timeout_(
            GetInputManagerImpl()->SetPowerButtonTimeoutMsForTest(1)) {}
  ~ScopedPowerButtonTimeoutShortener() {
    GetInputManagerImpl()->SetPowerButtonTimeoutMsForTest(original_timeout_);
  }

 private:
  InputManagerImpl* GetInputManagerImpl() {
    return static_cast<InputManagerImpl*>(InputManager::Get());
  }

  int original_timeout_;
  DISALLOW_COPY_AND_ASSIGN(ScopedPowerButtonTimeoutShortener);
};

}  // namespace test

TEST_F(InputManagerTest, PowerButton) {
  test::ScopedPowerButtonTimeoutShortener shortener;
  TestPowerButtonObserver observer;

  ui::test::EventGenerator generator(root_window());
  generator.PressKey(ui::VKEY_P, ui::EF_NONE);
  EXPECT_EQ(0, observer.count());

  // Test short press.
  generator.PressKey(ui::VKEY_P, ui::EF_ALT_DOWN);
  EXPECT_EQ(1, observer.count());
  EXPECT_EQ(PowerButtonObserver::PRESSED, observer.state());
  generator.ReleaseKey(ui::VKEY_P, ui::EF_ALT_DOWN);
  EXPECT_EQ(2, observer.count());
  EXPECT_EQ(PowerButtonObserver::RELEASED, observer.state());

  // Test long press.
  generator.PressKey(ui::VKEY_P, ui::EF_ALT_DOWN);
  EXPECT_EQ(3, observer.count());
  EXPECT_EQ(PowerButtonObserver::PRESSED, observer.state());

  EXPECT_TRUE(observer.WaitForLongPress());
  EXPECT_EQ(4, observer.count());
  EXPECT_EQ(PowerButtonObserver::LONG_PRESSED, observer.state());

  generator.ReleaseKey(ui::VKEY_P, ui::EF_ALT_DOWN);
  EXPECT_EQ(5, observer.count());
  EXPECT_EQ(PowerButtonObserver::RELEASED, observer.state());
}

}  // namespace athena
