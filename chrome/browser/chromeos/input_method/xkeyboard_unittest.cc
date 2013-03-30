// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/xkeyboard.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chromeos/ime/input_method_whitelist.h"
#include "chromeos/ime/mock_input_method_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <X11/Xlib.h>

namespace chromeos {
namespace input_method {

namespace {

class XKeyboardTest : public testing::Test {
 public:
  XKeyboardTest() {
  }

  virtual void SetUp() {
    xkey_.reset(XKeyboard::Create());
  }

  virtual void TearDown() {
    xkey_.reset();
  }

  InputMethodWhitelist whitelist_;
  scoped_ptr<XKeyboard> xkey_;

  MessageLoopForUI message_loop_;
};

// Returns true if X display is available.
bool DisplayAvailable() {
  return (base::MessagePumpForUI::GetDefaultXDisplay() != NULL);
}

}  // namespace

// Tests CheckLayoutName() function.
TEST_F(XKeyboardTest, TestCheckLayoutName) {
  // CheckLayoutName should not accept non-alphanumeric characters
  // except "()-_".
  EXPECT_FALSE(XKeyboard::CheckLayoutNameForTesting("us!"));
  EXPECT_FALSE(XKeyboard::CheckLayoutNameForTesting("us; /bin/sh"));
  EXPECT_TRUE(XKeyboard::CheckLayoutNameForTesting("ab-c_12"));

  // CheckLayoutName should not accept upper-case ascii characters.
  EXPECT_FALSE(XKeyboard::CheckLayoutNameForTesting("US"));

  // CheckLayoutName should accept lower-case ascii characters.
  for (int c = 'a'; c <= 'z'; ++c) {
    EXPECT_TRUE(XKeyboard::CheckLayoutNameForTesting(std::string(3, c)));
  }

  // CheckLayoutName should accept numbers.
  for (int c = '0'; c <= '9'; ++c) {
    EXPECT_TRUE(XKeyboard::CheckLayoutNameForTesting(std::string(3, c)));
  }

  // CheckLayoutName should accept a layout with a variant name.
  EXPECT_TRUE(XKeyboard::CheckLayoutNameForTesting("us(dvorak)"));
  EXPECT_TRUE(XKeyboard::CheckLayoutNameForTesting("jp"));
}

TEST_F(XKeyboardTest, TestSetCapsLockEnabled) {
  if (!DisplayAvailable()) {
    // Do not fail the test to allow developers to run unit_tests without an X
    // server (e.g. via ssh). Note that both try bots and waterfall always have
    // an X server for running browser_tests.
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  const bool initial_lock_state = xkey_->CapsLockIsEnabled();
  xkey_->SetCapsLockEnabled(true);
  EXPECT_TRUE(xkey_->CapsLockIsEnabled());
  xkey_->SetCapsLockEnabled(false);
  EXPECT_FALSE(xkey_->CapsLockIsEnabled());
  xkey_->SetCapsLockEnabled(true);
  EXPECT_TRUE(xkey_->CapsLockIsEnabled());
  xkey_->SetCapsLockEnabled(false);
  EXPECT_FALSE(xkey_->CapsLockIsEnabled());
  xkey_->SetCapsLockEnabled(initial_lock_state);
}

TEST_F(XKeyboardTest, TestSetNumLockEnabled) {
  if (!DisplayAvailable()) {
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  const unsigned int num_lock_mask = xkey_->GetNumLockMask();
  ASSERT_NE(0U, num_lock_mask);

  const bool initial_lock_state = xkey_->NumLockIsEnabled();
  xkey_->SetNumLockEnabled(true);
  EXPECT_TRUE(xkey_->NumLockIsEnabled());
  xkey_->SetNumLockEnabled(false);
  EXPECT_FALSE(xkey_->NumLockIsEnabled());
  xkey_->SetNumLockEnabled(true);
  EXPECT_TRUE(xkey_->NumLockIsEnabled());
  xkey_->SetNumLockEnabled(false);
  EXPECT_FALSE(xkey_->NumLockIsEnabled());
  xkey_->SetNumLockEnabled(initial_lock_state);
}

TEST_F(XKeyboardTest, TestSetCapsLockAndNumLockAtTheSameTime) {
  if (!DisplayAvailable()) {
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  const unsigned int num_lock_mask = xkey_->GetNumLockMask();
  ASSERT_NE(0U, num_lock_mask);

  const bool initial_caps_lock_state = xkey_->CapsLockIsEnabled();
  const bool initial_num_lock_state = xkey_->NumLockIsEnabled();

  // Flip both.
  xkey_->SetLockedModifiers(
      initial_caps_lock_state ? kDisableLock : kEnableLock,
      initial_num_lock_state ? kDisableLock : kEnableLock);
  EXPECT_EQ(!initial_caps_lock_state, xkey_->CapsLockIsEnabled());
  EXPECT_EQ(!initial_num_lock_state, xkey_->NumLockIsEnabled());

  // Flip Caps Lock.
  xkey_->SetLockedModifiers(
      initial_caps_lock_state ? kEnableLock : kDisableLock,
      kDontChange);
  // Use GetLockedModifiers() for verifying the result.
  bool c, n;
  xkey_->GetLockedModifiers(&c, &n);
  EXPECT_EQ(initial_caps_lock_state, c);
  EXPECT_EQ(!initial_num_lock_state, n);

  // Flip both.
  xkey_->SetLockedModifiers(
      initial_caps_lock_state ? kDisableLock : kEnableLock,
      initial_num_lock_state ? kEnableLock : kDisableLock);
  EXPECT_EQ(!initial_caps_lock_state, xkey_->CapsLockIsEnabled());
  EXPECT_EQ(initial_num_lock_state, xkey_->NumLockIsEnabled());

  // Flip Num Lock.
  xkey_->SetLockedModifiers(
      kDontChange,
      initial_num_lock_state ? kDisableLock : kEnableLock);
  xkey_->GetLockedModifiers(&c, &n);
  EXPECT_EQ(!initial_caps_lock_state, c);
  EXPECT_EQ(!initial_num_lock_state, n);

  // Flip both to restore the initial state.
  xkey_->SetLockedModifiers(
      initial_caps_lock_state ? kEnableLock : kDisableLock,
      initial_num_lock_state ? kEnableLock : kDisableLock);
  EXPECT_EQ(initial_caps_lock_state, xkey_->CapsLockIsEnabled());
  EXPECT_EQ(initial_num_lock_state, xkey_->NumLockIsEnabled());

  // No-op SetLockedModifiers call.
  xkey_->SetLockedModifiers(kDontChange, kDontChange);
  EXPECT_EQ(initial_caps_lock_state, xkey_->CapsLockIsEnabled());
  EXPECT_EQ(initial_num_lock_state, xkey_->NumLockIsEnabled());

  // No-op GetLockedModifiers call. Confirm it does not crash.
  xkey_->GetLockedModifiers(NULL, NULL);
}

TEST_F(XKeyboardTest, TestSetAutoRepeatEnabled) {
  if (!DisplayAvailable()) {
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  const bool state = XKeyboard::GetAutoRepeatEnabledForTesting();
  XKeyboard::SetAutoRepeatEnabled(!state);
  EXPECT_EQ(!state, XKeyboard::GetAutoRepeatEnabledForTesting());
  // Restore the initial state.
  XKeyboard::SetAutoRepeatEnabled(state);
  EXPECT_EQ(state, XKeyboard::GetAutoRepeatEnabledForTesting());
}

TEST_F(XKeyboardTest, TestSetAutoRepeatRate) {
  if (!DisplayAvailable()) {
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  AutoRepeatRate rate;
  EXPECT_TRUE(XKeyboard::GetAutoRepeatRateForTesting(&rate));

  AutoRepeatRate tmp(rate);
  ++tmp.initial_delay_in_ms;
  ++tmp.repeat_interval_in_ms;
  EXPECT_TRUE(XKeyboard::SetAutoRepeatRate(tmp));
  EXPECT_TRUE(XKeyboard::GetAutoRepeatRateForTesting(&tmp));
  EXPECT_EQ(rate.initial_delay_in_ms + 1, tmp.initial_delay_in_ms);
  EXPECT_EQ(rate.repeat_interval_in_ms + 1, tmp.repeat_interval_in_ms);

  // Restore the initial state.
  EXPECT_TRUE(XKeyboard::SetAutoRepeatRate(rate));
  EXPECT_TRUE(XKeyboard::GetAutoRepeatRateForTesting(&tmp));
  EXPECT_EQ(rate.initial_delay_in_ms, tmp.initial_delay_in_ms);
  EXPECT_EQ(rate.repeat_interval_in_ms, tmp.repeat_interval_in_ms);
}

}  // namespace input_method
}  // namespace chromeos
