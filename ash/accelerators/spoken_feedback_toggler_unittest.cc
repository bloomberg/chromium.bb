// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/spoken_feedback_toggler.h"
#include "ash/accessibility_delegate.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/events/test/event_generator.h"

namespace ash {

typedef ash::test::AshTestBase SpokenFeedbackTogglerTest;

TEST_F(SpokenFeedbackTogglerTest, Basic) {
  SpokenFeedbackToggler::ScopedEnablerForTest scoped;
  Shell* shell = Shell::GetInstance();
  AccessibilityDelegate* delegate = shell->accessibility_delegate();
  ui::test::EventGenerator& generator = GetEventGenerator();
  EXPECT_FALSE(delegate->IsSpokenFeedbackEnabled());

  generator.PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(delegate->IsSpokenFeedbackEnabled());
  generator.ReleaseKey(ui::VKEY_F6, 0);
  EXPECT_FALSE(delegate->IsSpokenFeedbackEnabled());

  // Click and hold toggles the spoken feedback.
  generator.PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(delegate->IsSpokenFeedbackEnabled());
  generator.PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(delegate->IsSpokenFeedbackEnabled());
  generator.PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(delegate->IsSpokenFeedbackEnabled());
  generator.ReleaseKey(ui::VKEY_F6, 0);
  EXPECT_TRUE(delegate->IsSpokenFeedbackEnabled());

  // toggle again
  generator.PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(delegate->IsSpokenFeedbackEnabled());
  generator.PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(delegate->IsSpokenFeedbackEnabled());
  generator.PressKey(ui::VKEY_F6, ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(delegate->IsSpokenFeedbackEnabled());
  generator.ReleaseKey(ui::VKEY_F6, 0);
  EXPECT_FALSE(delegate->IsSpokenFeedbackEnabled());
}

}  // namespace ash
