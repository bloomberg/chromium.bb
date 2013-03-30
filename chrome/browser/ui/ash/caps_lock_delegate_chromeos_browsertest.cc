// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/caps_lock_delegate_chromeos.h"

#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ime/mock_xkeyboard.h"

namespace {

class CapsLockDelegateTest : public InProcessBrowserTest {
 public:
  CapsLockDelegateTest() : initial_caps_lock_state_(false) {
  }

  virtual void SetUp() OVERRIDE {
    delegate_.reset(new CapsLockDelegate(&xkeyboard_));
    // Force CapsLockDelegate::ToggleCapsLock() to toggle the lock state.
    delegate_->set_is_running_on_chromeos_for_test(true);
  }
  virtual void TearDown() OVERRIDE {
    delegate_.reset();
  }

 protected:
  const bool initial_caps_lock_state_;
  chromeos::input_method::MockXKeyboard xkeyboard_;
  scoped_ptr<CapsLockDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockDelegateTest);
};

// Check if ToggleCapsLock() really changes the lock state.
IN_PROC_BROWSER_TEST_F(CapsLockDelegateTest, TestCapsLock) {
  EXPECT_EQ(initial_caps_lock_state_, delegate_->caps_lock_is_on_for_test());
  delegate_->ToggleCapsLock();
  EXPECT_EQ(!initial_caps_lock_state_, xkeyboard_.CapsLockIsEnabled());
  delegate_->OnCapsLockChange(!initial_caps_lock_state_);
  EXPECT_EQ(!initial_caps_lock_state_, delegate_->caps_lock_is_on_for_test());
  delegate_->ToggleCapsLock();
  delegate_->OnCapsLockChange(initial_caps_lock_state_);
  EXPECT_EQ(initial_caps_lock_state_, xkeyboard_.CapsLockIsEnabled());
  EXPECT_EQ(initial_caps_lock_state_, delegate_->caps_lock_is_on_for_test());

  // Check if SetCapsLockEnabled really changes the lock state.
  delegate_->SetCapsLockEnabled(!initial_caps_lock_state_);
  EXPECT_EQ(!initial_caps_lock_state_, delegate_->caps_lock_is_on_for_test());
  EXPECT_EQ(!initial_caps_lock_state_, xkeyboard_.CapsLockIsEnabled());
  EXPECT_EQ(!initial_caps_lock_state_, delegate_->IsCapsLockEnabled());
  delegate_->SetCapsLockEnabled(initial_caps_lock_state_);
  EXPECT_EQ(initial_caps_lock_state_, delegate_->caps_lock_is_on_for_test());
  EXPECT_EQ(initial_caps_lock_state_, xkeyboard_.CapsLockIsEnabled());
  EXPECT_EQ(initial_caps_lock_state_, delegate_->IsCapsLockEnabled());
}

}  // namespace
