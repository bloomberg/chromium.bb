// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "chrome/browser/chromeos/caps_lock_rewriter.h"
#include "chromeos/ime/mock_xkeyboard.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/events/event.h"

namespace chromeos {

class CapsLockRewriterTest : public testing::Test {
 public:
  CapsLockRewriterTest() {}
  virtual ~CapsLockRewriterTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    now_ = base::TimeTicks::Now();
    caps_lock_rewriter_.set_xkeyboard_for_testing(&xkeyboard_);
  }

  void KeyPressed(ui::KeyboardCode key_code) {
    ui::KeyEvent key_event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE, false);
    caps_lock_rewriter_.RewriteEvent(&key_event, now_);
  }

  void KeyReleased(ui::KeyboardCode key_code) {
    ui::KeyEvent key_event(ui::ET_KEY_RELEASED, key_code, ui::EF_NONE, false);
    caps_lock_rewriter_.RewriteEvent(&key_event, now_);
  }

  void TimeFlies(const base::TimeDelta duration) {
    now_ += duration;
  }

  bool CapsLockOn() { return xkeyboard_.CapsLockIsEnabled(); }
  ui::KeyboardCode trigger_key() { return caps_lock_rewriter_.trigger_key(); }

 private:
  base::TimeTicks now_;

  CapsLockRewriter caps_lock_rewriter_;
  chromeos::input_method::MockXKeyboard xkeyboard_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockRewriterTest);
};

// Test that caps lock is turned off on the next key.
TEST_F(CapsLockRewriterTest, AutoReset) {
  EXPECT_FALSE(CapsLockOn());

  // Simple case.
  KeyPressed(trigger_key());
  KeyReleased(trigger_key());
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_A);
  KeyReleased(ui::VKEY_A);
  EXPECT_FALSE(CapsLockOn());

  // Press trigger key again but not triggering double press.
  KeyPressed(trigger_key());
  KeyReleased(trigger_key());
  EXPECT_TRUE(CapsLockOn());

  TimeFlies(base::TimeDelta::FromMilliseconds(
      CapsLockRewriter::kDefaultDoublePressDurationInMs + 1));

  KeyPressed(trigger_key());
  KeyReleased(trigger_key());
  EXPECT_FALSE(CapsLockOn());

  // Modifier key release after char key release.
  KeyPressed(trigger_key());
  KeyReleased(trigger_key());
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_SHIFT);
  KeyPressed(ui::VKEY_A);
  KeyReleased(ui::VKEY_A);
  KeyPressed(ui::VKEY_SHIFT);
  EXPECT_FALSE(CapsLockOn());

  // Modifier key release before char key release.
  KeyPressed(trigger_key());
  KeyReleased(trigger_key());
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_SHIFT);
  KeyPressed(ui::VKEY_A);
  KeyPressed(ui::VKEY_SHIFT);
  KeyReleased(ui::VKEY_A);
  EXPECT_FALSE(CapsLockOn());
}

// Test that the trigger key works like SHIFT key, i.e. caps lock is on while
// holding down the trigger key.
TEST_F(CapsLockRewriterTest, ShiftAlike) {
  EXPECT_FALSE(CapsLockOn());

  KeyPressed(trigger_key());
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_A);
  KeyReleased(ui::VKEY_A);
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_B);
  KeyReleased(ui::VKEY_B);
  EXPECT_TRUE(CapsLockOn());

  // Modifier key release after char key release.
  KeyPressed(ui::VKEY_SHIFT);
  KeyPressed(ui::VKEY_A);
  KeyReleased(ui::VKEY_A);
  KeyPressed(ui::VKEY_SHIFT);
  EXPECT_TRUE(CapsLockOn());

  // Modifier key release before char key release.
  KeyPressed(ui::VKEY_SHIFT);
  KeyPressed(ui::VKEY_A);
  KeyPressed(ui::VKEY_SHIFT);
  KeyReleased(ui::VKEY_A);
  EXPECT_TRUE(CapsLockOn());

  KeyReleased(trigger_key());
  EXPECT_FALSE(CapsLockOn());
}

TEST_F(CapsLockRewriterTest, TriggerKeyReleaseBeforeCharKey) {
  KeyPressed(trigger_key());
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_A);

  KeyReleased(trigger_key());
  EXPECT_FALSE(CapsLockOn());

  KeyReleased(ui::VKEY_A);
  EXPECT_FALSE(CapsLockOn());
}

// Test that long pressing the trigger key works like CAPS key, i.e. caps
// lock is turned on until the trigger key is pressed again.
TEST_F(CapsLockRewriterTest, LongPressLock) {
  EXPECT_FALSE(CapsLockOn());

  KeyPressed(trigger_key());

  TimeFlies(base::TimeDelta::FromMilliseconds(
      CapsLockRewriter::kDefaultLongPressDurationInMs + 1));

  KeyReleased(trigger_key());
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_A);
  KeyReleased(ui::VKEY_A);
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_B);
  KeyReleased(ui::VKEY_B);
  EXPECT_TRUE(CapsLockOn());

  // Modifier key release after char key release.
  KeyPressed(ui::VKEY_SHIFT);
  KeyPressed(ui::VKEY_A);
  KeyReleased(ui::VKEY_A);
  KeyPressed(ui::VKEY_SHIFT);
  EXPECT_TRUE(CapsLockOn());

  // Modifier key release before char key release.
  KeyPressed(ui::VKEY_SHIFT);
  KeyPressed(ui::VKEY_A);
  KeyPressed(ui::VKEY_SHIFT);
  KeyReleased(ui::VKEY_A);
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(trigger_key());
  KeyReleased(trigger_key());
  EXPECT_FALSE(CapsLockOn());
}

// Test that the 2nd long press clears the caps lock.
TEST_F(CapsLockRewriterTest, LongPressAfterLongPress) {
  EXPECT_FALSE(CapsLockOn());

  KeyPressed(trigger_key());
  TimeFlies(base::TimeDelta::FromMilliseconds(
      CapsLockRewriter::kDefaultLongPressDurationInMs + 1));
  KeyReleased(trigger_key());
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(trigger_key());
  TimeFlies(base::TimeDelta::FromMilliseconds(
      CapsLockRewriter::kDefaultLongPressDurationInMs + 1));
  KeyReleased(trigger_key());
  EXPECT_FALSE(CapsLockOn());
}

// Similar to LongPressLock but test double pressing.
TEST_F(CapsLockRewriterTest, DoublePressLock) {
  EXPECT_FALSE(CapsLockOn());

  KeyPressed(trigger_key());
  KeyReleased(trigger_key());
  KeyPressed(trigger_key());
  KeyReleased(trigger_key());
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_A);
  KeyReleased(ui::VKEY_A);
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_B);
  KeyReleased(ui::VKEY_B);
  EXPECT_TRUE(CapsLockOn());

  // Modifier key release after char key release.
  KeyPressed(ui::VKEY_SHIFT);
  KeyPressed(ui::VKEY_A);
  KeyReleased(ui::VKEY_A);
  KeyPressed(ui::VKEY_SHIFT);
  EXPECT_TRUE(CapsLockOn());

  // Modifier key release before char key release.
  KeyPressed(ui::VKEY_SHIFT);
  KeyPressed(ui::VKEY_A);
  KeyPressed(ui::VKEY_SHIFT);
  KeyReleased(ui::VKEY_A);
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(trigger_key());
  KeyReleased(trigger_key());
  EXPECT_FALSE(CapsLockOn());
}

// Test that the 2nd trigger key press in a double press works as SHIFT if user
// starts typing without releasing and caps lock is cleared when its released.
TEST_F(CapsLockRewriterTest, DoublePressShiftAlive) {
  EXPECT_FALSE(CapsLockOn());

  KeyPressed(trigger_key());
  KeyReleased(trigger_key());
  KeyPressed(trigger_key());
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_A);
  KeyReleased(ui::VKEY_A);
  EXPECT_TRUE(CapsLockOn());

  KeyPressed(ui::VKEY_B);
  KeyReleased(ui::VKEY_B);
  EXPECT_TRUE(CapsLockOn());

  // Modifier key release after char key release.
  KeyPressed(ui::VKEY_SHIFT);
  KeyPressed(ui::VKEY_A);
  KeyReleased(ui::VKEY_A);
  KeyPressed(ui::VKEY_SHIFT);
  EXPECT_TRUE(CapsLockOn());

  // Modifier key release before char key release.
  KeyPressed(ui::VKEY_SHIFT);
  KeyPressed(ui::VKEY_A);
  KeyPressed(ui::VKEY_SHIFT);
  KeyReleased(ui::VKEY_A);
  EXPECT_TRUE(CapsLockOn());

  KeyReleased(trigger_key());
  EXPECT_FALSE(CapsLockOn());
}

}  // namespace chromeos
