// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/ime_keyboard.h"

#include <algorithm>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/x/x11_types.h"

#include <X11/Xlib.h>

namespace chromeos {
namespace input_method {

namespace {

class ImeKeyboardTest : public testing::Test,
                        public ImeKeyboard::Observer {
 public:
  ImeKeyboardTest() {
  }

  virtual void SetUp() {
    xkey_.reset(ImeKeyboard::Create());
    xkey_->AddObserver(this);
    caps_changed_ = false;
  }

  virtual void TearDown() {
    xkey_->RemoveObserver(this);
    xkey_.reset();
  }

  virtual void OnCapsLockChanged(bool enabled) OVERRIDE {
    caps_changed_ = true;
  }

  void VerifyCapsLockChanged(bool changed) {
    EXPECT_EQ(changed, caps_changed_);
    caps_changed_ = false;
  }

  scoped_ptr<ImeKeyboard> xkey_;
  base::MessageLoopForUI message_loop_;
  bool caps_changed_;
};

// Returns true if X display is available.
bool DisplayAvailable() {
  return gfx::GetXDisplay() != NULL;
}

}  // namespace

// Tests CheckLayoutName() function.
TEST_F(ImeKeyboardTest, TestCheckLayoutName) {
  // CheckLayoutName should not accept non-alphanumeric characters
  // except "()-_".
  EXPECT_FALSE(ImeKeyboard::CheckLayoutNameForTesting("us!"));
  EXPECT_FALSE(ImeKeyboard::CheckLayoutNameForTesting("us; /bin/sh"));
  EXPECT_TRUE(ImeKeyboard::CheckLayoutNameForTesting("ab-c_12"));

  // CheckLayoutName should not accept upper-case ascii characters.
  EXPECT_FALSE(ImeKeyboard::CheckLayoutNameForTesting("US"));

  // CheckLayoutName should accept lower-case ascii characters.
  for (int c = 'a'; c <= 'z'; ++c) {
    EXPECT_TRUE(ImeKeyboard::CheckLayoutNameForTesting(std::string(3, c)));
  }

  // CheckLayoutName should accept numbers.
  for (int c = '0'; c <= '9'; ++c) {
    EXPECT_TRUE(ImeKeyboard::CheckLayoutNameForTesting(std::string(3, c)));
  }

  // CheckLayoutName should accept a layout with a variant name.
  EXPECT_TRUE(ImeKeyboard::CheckLayoutNameForTesting("us(dvorak)"));
  EXPECT_TRUE(ImeKeyboard::CheckLayoutNameForTesting("jp"));
}

TEST_F(ImeKeyboardTest, TestSetCapsLockEnabled) {
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
  VerifyCapsLockChanged(true);

  xkey_->SetCapsLockEnabled(true);
  EXPECT_TRUE(xkey_->CapsLockIsEnabled());
  VerifyCapsLockChanged(true);

  xkey_->SetCapsLockEnabled(false);
  EXPECT_FALSE(xkey_->CapsLockIsEnabled());
  VerifyCapsLockChanged(true);

  xkey_->SetCapsLockEnabled(false);
  EXPECT_FALSE(xkey_->CapsLockIsEnabled());
  VerifyCapsLockChanged(false);

  xkey_->SetCapsLockEnabled(initial_lock_state);
}

TEST_F(ImeKeyboardTest, TestSetAutoRepeatEnabled) {
  if (!DisplayAvailable()) {
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  const bool state = ImeKeyboard::GetAutoRepeatEnabledForTesting();
  xkey_->SetAutoRepeatEnabled(!state);
  EXPECT_EQ(!state, ImeKeyboard::GetAutoRepeatEnabledForTesting());
  // Restore the initial state.
  xkey_->SetAutoRepeatEnabled(state);
  EXPECT_EQ(state, ImeKeyboard::GetAutoRepeatEnabledForTesting());
}

TEST_F(ImeKeyboardTest, TestSetAutoRepeatRate) {
  if (!DisplayAvailable()) {
    DVLOG(1) << "X server is not available. Skip the test.";
    return;
  }
  AutoRepeatRate rate;
  EXPECT_TRUE(ImeKeyboard::GetAutoRepeatRateForTesting(&rate));

  AutoRepeatRate tmp(rate);
  ++tmp.initial_delay_in_ms;
  ++tmp.repeat_interval_in_ms;
  EXPECT_TRUE(xkey_->SetAutoRepeatRate(tmp));
  EXPECT_TRUE(ImeKeyboard::GetAutoRepeatRateForTesting(&tmp));
  EXPECT_EQ(rate.initial_delay_in_ms + 1, tmp.initial_delay_in_ms);
  EXPECT_EQ(rate.repeat_interval_in_ms + 1, tmp.repeat_interval_in_ms);

  // Restore the initial state.
  EXPECT_TRUE(xkey_->SetAutoRepeatRate(rate));
  EXPECT_TRUE(ImeKeyboard::GetAutoRepeatRateForTesting(&tmp));
  EXPECT_EQ(rate.initial_delay_in_ms, tmp.initial_delay_in_ms);
  EXPECT_EQ(rate.repeat_interval_in_ms, tmp.repeat_interval_in_ms);
}

}  // namespace input_method
}  // namespace chromeos
