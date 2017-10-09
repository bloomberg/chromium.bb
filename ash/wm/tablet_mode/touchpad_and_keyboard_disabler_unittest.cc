// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/touchpad_and_keyboard_disabler.h"

#include <memory>
#include <utility>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_environment.h"
#include "ash/test/ash_test_helper.h"
#include "base/bind.h"

namespace ash {
namespace {

class TestDelegate : public TouchpadAndKeyboardDisabler::Delegate {
 public:
  TestDelegate(bool* destroyed,
               int* hide_cursor_call_count,
               int* enable_call_count,
               int* disable_call_count)
      : destroyed_(destroyed),
        hide_cursor_call_count_(hide_cursor_call_count),
        enable_call_count_(enable_call_count),
        disable_call_count_(disable_call_count) {}
  ~TestDelegate() override { *destroyed_ = true; }

  DisableResultClosure disable_callback() {
    return std::move(disable_callback_);
  }

  bool is_disable_callback_valid() const {
    return !disable_callback_.is_null();
  }

  void RunDisableCallback(bool result) {
    std::move(disable_callback_).Run(result);
  }

  // TouchpadAndKeyboardDisabler::Delegate:
  void Disable(DisableResultClosure callback) override {
    disable_callback_ = std::move(callback);
    (*disable_call_count_)++;
  }
  void HideCursor() override { (*hide_cursor_call_count_)++; }
  void Enable() override { (*enable_call_count_)++; }

 private:
  bool* destroyed_;
  DisableResultClosure disable_callback_;
  int* hide_cursor_call_count_;
  int* enable_call_count_;
  int* disable_call_count_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

struct CallCounts {
  int hide_cursor_call_count = 0;
  int enable_call_count = 0;
  int disable_call_count = 0;
  bool delegate_destroyed = false;
};

TouchpadAndKeyboardDisabler* CreateDisablerAndDelegate(
    CallCounts* counts,
    TestDelegate** test_delegate) {
  std::unique_ptr<TestDelegate> owned_test_delegate =
      std::make_unique<TestDelegate>(
          &(counts->delegate_destroyed), &(counts->hide_cursor_call_count),
          &(counts->enable_call_count), &(counts->disable_call_count));
  *test_delegate = owned_test_delegate.get();
  return new TouchpadAndKeyboardDisabler(std::move(owned_test_delegate));
}

}  // namespace

class TouchpadAndKeyboardDisablerTest : public AshTestBase {
 public:
  TouchpadAndKeyboardDisablerTest() {}

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    disabler_ = CreateDisablerAndDelegate(&counts_, &test_delegate_);
  }

  void TearDown() override {
    EXPECT_TRUE(counts_.delegate_destroyed);
    AshTestBase::TearDown();
  }

 protected:
  CallCounts counts_;
  // See class description for ownership details.
  TouchpadAndKeyboardDisabler* disabler_ = nullptr;
  // Owned by |disabler_|.
  TestDelegate* test_delegate_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchpadAndKeyboardDisablerTest);
};

TEST_F(TouchpadAndKeyboardDisablerTest, DisableCallbackSucceeds) {
  ASSERT_TRUE(test_delegate_->is_disable_callback_valid());
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);

  test_delegate_->RunDisableCallback(true);
  EXPECT_EQ(1, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);

  disabler_->Destroy();
  EXPECT_EQ(1, counts_.hide_cursor_call_count);
  EXPECT_EQ(1, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);
  EXPECT_TRUE(counts_.delegate_destroyed);
}

TEST_F(TouchpadAndKeyboardDisablerTest, DisableCallbackFails) {
  ASSERT_TRUE(test_delegate_->is_disable_callback_valid());
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);

  test_delegate_->RunDisableCallback(false);
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);

  disabler_->Destroy();
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);
  EXPECT_TRUE(counts_.delegate_destroyed);
}

TEST_F(TouchpadAndKeyboardDisablerTest, DisableCallbackSucceedsAfterDestroy) {
  ASSERT_TRUE(test_delegate_->is_disable_callback_valid());
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);

  disabler_->Destroy();
  ASSERT_FALSE(counts_.delegate_destroyed);

  test_delegate_->RunDisableCallback(true);
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(1, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);
  EXPECT_TRUE(counts_.delegate_destroyed);
}

TEST_F(TouchpadAndKeyboardDisablerTest, DisableCallbackFailsAfterDestroy) {
  ASSERT_TRUE(test_delegate_->is_disable_callback_valid());
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);

  disabler_->Destroy();
  ASSERT_FALSE(counts_.delegate_destroyed);

  test_delegate_->RunDisableCallback(false);
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);
  EXPECT_TRUE(counts_.delegate_destroyed);
}

TEST_F(TouchpadAndKeyboardDisablerTest, CallbackSucceedsAfterDestroy) {
  ASSERT_TRUE(test_delegate_->is_disable_callback_valid());
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);

  disabler_->Destroy();
  ASSERT_FALSE(counts_.delegate_destroyed);

  test_delegate_->RunDisableCallback(true);
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(1, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);
  ASSERT_TRUE(counts_.delegate_destroyed);
}

TEST_F(TouchpadAndKeyboardDisablerTest, CallbackFailsAfterDestroy) {
  ASSERT_TRUE(test_delegate_->is_disable_callback_valid());
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);

  disabler_->Destroy();
  ASSERT_FALSE(counts_.delegate_destroyed);

  test_delegate_->RunDisableCallback(false);
  EXPECT_EQ(0, counts_.hide_cursor_call_count);
  EXPECT_EQ(0, counts_.enable_call_count);
  EXPECT_EQ(1, counts_.disable_call_count);
  ASSERT_TRUE(counts_.delegate_destroyed);
}

using TouchpadAndKeyboardDisablerTest2 = testing::Test;

TEST_F(TouchpadAndKeyboardDisablerTest2,
       DisableCallbackSucceedsAfterShellDestroyed) {
  std::unique_ptr<AshTestEnvironment> ash_test_environment =
      AshTestEnvironment::Create();
  std::unique_ptr<AshTestHelper> ash_test_helper =
      std::make_unique<AshTestHelper>(ash_test_environment.get());
  const bool start_session = true;
  ash_test_helper->SetUp(start_session);

  CallCounts counts;
  // Owned by |disabler_|.
  TestDelegate* test_delegate = nullptr;
  // See class descripition for details on ownership.
  TouchpadAndKeyboardDisabler* disabler =
      CreateDisablerAndDelegate(&counts, &test_delegate);

  ASSERT_TRUE(test_delegate->is_disable_callback_valid());
  TestDelegate::DisableResultClosure disable_result_closure =
      test_delegate->disable_callback();
  EXPECT_EQ(0, counts.hide_cursor_call_count);
  EXPECT_EQ(0, counts.enable_call_count);
  EXPECT_EQ(1, counts.disable_call_count);

  disabler->Destroy();
  ash_test_helper->RunAllPendingInMessageLoop();
  ash_test_helper->TearDown();
  ash_test_helper.reset();
  ASSERT_FALSE(Shell::HasInstance());
  EXPECT_TRUE(counts.delegate_destroyed);
  EXPECT_EQ(0, counts.hide_cursor_call_count);
  EXPECT_EQ(0, counts.enable_call_count);
  EXPECT_EQ(1, counts.disable_call_count);

  // Run the callback, just to be sure nothing bad happens.
  std::move(disable_result_closure).Run(true);
  EXPECT_TRUE(counts.delegate_destroyed);
  EXPECT_EQ(0, counts.hide_cursor_call_count);
  EXPECT_EQ(0, counts.enable_call_count);
  EXPECT_EQ(1, counts.disable_call_count);
}

}  // namespace ash
