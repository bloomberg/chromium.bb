// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_events.h"
#include "chrome/browser/chromeos/login/wizard_accessibility_handler.h"
#include "chrome/common/chrome_notification_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::EarconType;
using chromeos::WizardAccessibilityHandler;

namespace chromeos {

class WizardAccessibilityHandlerTest : public testing::Test {
 protected:
  void ChangeText(WizardAccessibilityHandler* handler,
                  AccessibilityTextBoxInfo* textbox_info,
                  const std::string& value,
                  int selection_start,
                  int selection_end,
                  std::string* description) {
    textbox_info->SetValue(value, selection_start, selection_end);
    EarconType earcon = chromeos::NO_EARCON;
    handler->DescribeAccessibilityEvent(
        chrome::NOTIFICATION_ACCESSIBILITY_TEXT_CHANGED,
        textbox_info,
        description,
        &earcon);
  }
};

TEST_F(WizardAccessibilityHandlerTest, TestFocusEvents) {
  WizardAccessibilityHandler handler;

  std::string description;
  EarconType earcon;

  // No need to test every possible control, but test several types
  // to exercise different types of string concatenation.

  // NOTE: unittests are forced to run under the en-US locale, so it's
  // safe to do these string comparisons without using l10n_util.

  // Test a simple control.
  std::string button_name = "Save";
  AccessibilityButtonInfo button_info(NULL, button_name);
  handler.DescribeAccessibilityEvent(
      chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED,
      &button_info,
      &description,
      &earcon);
  EXPECT_EQ(chromeos::EARCON_BUTTON, earcon);
  EXPECT_EQ("Save Button", description);

  // Test a control with multiple states.
  std::string checkbox_name = "Accessibility";
  AccessibilityCheckboxInfo checkbox_info(NULL, checkbox_name, false);
  handler.DescribeAccessibilityEvent(
      chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED,
      &checkbox_info,
      &description,
      &earcon);
  EXPECT_EQ(chromeos::EARCON_CHECK_OFF, earcon);
  EXPECT_EQ("Accessibility Unchecked check box", description);
  checkbox_info.SetChecked(true);
  handler.DescribeAccessibilityEvent(
      chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED,
      &checkbox_info,
      &description,
      &earcon);
  EXPECT_EQ(chromeos::EARCON_CHECK_ON, earcon);
  EXPECT_EQ("Accessibility Checked check box", description);

  // Test a control with a value and index.
  std::string combobox_name = "Language";
  std::string combobox_value = "English";
  AccessibilityComboBoxInfo combobox_info(
      NULL, combobox_name, combobox_value, 12, 35);
  handler.DescribeAccessibilityEvent(
      chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED,
      &combobox_info,
      &description,
      &earcon);
  EXPECT_EQ(chromeos::EARCON_LISTBOX, earcon);
  EXPECT_EQ("English Language Combo box 13 of 35", description);
}

TEST_F(WizardAccessibilityHandlerTest, TestTextEvents) {
  WizardAccessibilityHandler handler;

  std::string description;
  EarconType earcon;

  AccessibilityTextBoxInfo textbox_info(NULL, "", false);
  handler.DescribeAccessibilityEvent(
      chrome::NOTIFICATION_ACCESSIBILITY_CONTROL_FOCUSED,
      &textbox_info,
      &description,
      &earcon);
  EXPECT_EQ("Text box", description);
  EXPECT_EQ(chromeos::EARCON_TEXTBOX, earcon);

  // Type "hello world.", one character at a time.
  ChangeText(&handler, &textbox_info, "h", 1, 1, &description);
  EXPECT_EQ("h", description);
  ChangeText(&handler, &textbox_info, "he", 2, 2, &description);
  EXPECT_EQ("e", description);
  ChangeText(&handler, &textbox_info, "hel", 3, 3, &description);
  EXPECT_EQ("l", description);
  ChangeText(&handler, &textbox_info, "hell", 4, 4, &description);
  EXPECT_EQ("l", description);
  ChangeText(&handler, &textbox_info, "hello", 5, 5, &description);
  EXPECT_EQ("o", description);
  ChangeText(&handler, &textbox_info, "hello ", 6, 6, &description);
  EXPECT_EQ("Space", description);
  ChangeText(&handler, &textbox_info, "hello w", 7, 7, &description);
  EXPECT_EQ("w", description);
  ChangeText(&handler, &textbox_info, "hello wo", 8, 8, &description);
  EXPECT_EQ("o", description);
  ChangeText(&handler, &textbox_info, "hello wor", 9, 9, &description);
  EXPECT_EQ("r", description);
  ChangeText(&handler, &textbox_info, "hello worl", 10, 10, &description);
  EXPECT_EQ("l", description);
  ChangeText(&handler, &textbox_info, "hello world", 11, 11, &description);
  EXPECT_EQ("d", description);
  ChangeText(&handler, &textbox_info, "hello world.", 12, 12, &description);
  EXPECT_EQ("Period", description);

  // Move by characters and by words.
  ChangeText(&handler, &textbox_info, "hello world.", 11, 11, &description);
  EXPECT_EQ("Period", description);
  ChangeText(&handler, &textbox_info, "hello world.", 6, 6, &description);
  EXPECT_EQ("world", description);
  ChangeText(&handler, &textbox_info, "hello world.", 0, 0, &description);
  EXPECT_EQ("hello ", description);
  ChangeText(&handler, &textbox_info, "hello world.", 1, 1, &description);
  EXPECT_EQ("h", description);
  ChangeText(&handler, &textbox_info, "hello world.", 5, 5, &description);
  EXPECT_EQ("ello", description);

  // Delete characters and words.
  ChangeText(&handler, &textbox_info, "hell world.", 4, 4, &description);
  EXPECT_EQ("o", description);
  ChangeText(&handler, &textbox_info, "hel world.", 3, 3, &description);
  EXPECT_EQ("l", description);
  ChangeText(&handler, &textbox_info, " world.", 0, 0, &description);
  EXPECT_EQ("hel", description);

  // Select characters and words.
  ChangeText(&handler, &textbox_info, " world.", 0, 1, &description);
  EXPECT_EQ("Space", description);
  ChangeText(&handler, &textbox_info, " world.", 0, 4, &description);
  EXPECT_EQ("wor", description);
  ChangeText(&handler, &textbox_info, " world.", 1, 4, &description);
  EXPECT_EQ("Space", description);
  ChangeText(&handler, &textbox_info, " world.", 4, 4, &description);
  EXPECT_EQ("Unselected", description);

  // If the string suddenly changes, it should just speak the new value.
  ChangeText(&handler, &textbox_info, "Potato", 0, 0, &description);
  EXPECT_EQ("Potato", description);
}

}
