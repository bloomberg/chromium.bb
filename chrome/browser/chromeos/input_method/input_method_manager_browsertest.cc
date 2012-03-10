// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_manager.h"

#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/keycodes/keyboard_codes.h"

namespace chromeos {
namespace input_method {

class InputMethodManagerTest : public InProcessBrowserTest {
 public:
  InputMethodManagerTest() : manager_(InputMethodManager::GetInstance()) {}

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    manager_->SetEnableAutoImeShutdown(true);
  }
  virtual void CleanUpOnMainThread() OVERRIDE {
    manager_->EnableInputMethods("en-US", kKeyboardLayoutsOnly, "xkb:us::eng");
    manager_->StopInputMethodDaemon();
  }

  InputMethodManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodManagerTest);
};

IN_PROC_BROWSER_TEST_F(InputMethodManagerTest, TestEnableInputMethods) {
  // Currently 5 keyboard layouts are supported for en-US, and 1 for ja. See
  // ibus_input_method.txt.
  manager_->EnableInputMethods("en-US", kKeyboardLayoutsOnly, "");
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  // The hardware keyboard layout "xkb:us::eng" is always active, hence 2U.
  manager_->EnableInputMethods("ja", kKeyboardLayoutsOnly, "");
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
}

IN_PROC_BROWSER_TEST_F(InputMethodManagerTest, TestNextInputMethod) {
  manager_->EnableInputMethods("en-US", kKeyboardLayoutsOnly, "xkb:us::eng");
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ("xkb:us:intl:eng", manager_->current_input_method().id());
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ("xkb:us:altgr-intl:eng", manager_->current_input_method().id());
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ("xkb:us:dvorak:eng", manager_->current_input_method().id());
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ("xkb:us:colemak:eng", manager_->current_input_method().id());
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
}

IN_PROC_BROWSER_TEST_F(InputMethodManagerTest, TestPreviousInputMethod) {
  manager_->EnableInputMethods("en-US", kKeyboardLayoutsOnly, "xkb:us::eng");
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ("xkb:us:intl:eng", manager_->current_input_method().id());
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us:intl:eng", manager_->current_input_method().id());
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ("xkb:us:intl:eng", manager_->current_input_method().id());
  manager_->SwitchToNextInputMethod();
  EXPECT_EQ("xkb:us:altgr-intl:eng", manager_->current_input_method().id());
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us:intl:eng", manager_->current_input_method().id());
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us:altgr-intl:eng", manager_->current_input_method().id());
}

IN_PROC_BROWSER_TEST_F(InputMethodManagerTest, TestSwitchInputMethod) {
  manager_->EnableInputMethods("en-US", kKeyboardLayoutsOnly, "xkb:us::eng");
  EXPECT_EQ(5U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());

  // Henkan, Muhenkan, ZenkakuHankaku should be ignored when no Japanese IMEs
  // and keyboards are enabled.
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_CONVERT, false, false, false)));
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_NONCONVERT, false, false, false)));
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_SBCSCHAR, false, false, false)));
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_DBCSCHAR, false, false, false)));
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());

  // Do the same tests for Korean.
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_HANGUL, false, false, false)));
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  EXPECT_FALSE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_SPACE, true, false, false)));
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());

  // Enable "xkb:jp::jpn" and press Muhenkan/ZenkakuHankaku.
  manager_->EnableInputMethods("ja", kKeyboardLayoutsOnly, "xkb:us::eng");
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_NONCONVERT, false, false, false)));
  EXPECT_EQ("xkb:jp::jpn", manager_->current_input_method().id());
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_SBCSCHAR, false, false, false)));
  EXPECT_EQ("xkb:jp::jpn", manager_->current_input_method().id());
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_DBE_DBCSCHAR, false, false, false)));
  EXPECT_EQ("xkb:jp::jpn", manager_->current_input_method().id());

  // Do the same tests for Korean.
  manager_->EnableInputMethods("ko", kKeyboardLayoutsOnly, "xkb:us::eng");
  EXPECT_EQ(2U, manager_->GetNumActiveInputMethods());
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_HANGUL, false, false, false)));
  EXPECT_EQ("xkb:kr:kr104:kor", manager_->current_input_method().id());
  manager_->SwitchToPreviousInputMethod();
  EXPECT_EQ("xkb:us::eng", manager_->current_input_method().id());
  EXPECT_TRUE(manager_->SwitchInputMethod(
      ui::Accelerator(ui::VKEY_SPACE, true, false, false)));
  EXPECT_EQ("xkb:kr:kr104:kor", manager_->current_input_method().id());
}

}  // namespace input_method
}  // namespace chromeos
