// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/password_generation_popup_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/password_manager/core/browser/password_generation_manager.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

class TestPopupObserver : public autofill::PasswordGenerationPopupObserver {
 public:
  TestPopupObserver()
      : popup_showing_(false),
        password_visible_(false) {}
  virtual ~TestPopupObserver() {}

  void OnPopupShown(bool password_visible) override {
    popup_showing_ = true;
    password_visible_ = password_visible;
  }

  void OnPopupHidden() override { popup_showing_ = false; }

  bool popup_showing() { return popup_showing_; }
  bool password_visible() { return password_visible_; }

 private:
  bool popup_showing_;
  bool password_visible_;
};

}  // namespace

class PasswordGenerationInteractiveTest :
    public PasswordManagerBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make sure the feature is enabled.
    command_line->AppendSwitch(autofill::switches::kEnablePasswordGeneration);

    // Don't require ping from autofill or blacklist checking.
    command_line->AppendSwitch(
        autofill::switches::kLocalHeuristicsOnlyForPasswordGeneration);
  }

  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::SetUpOnMainThread();

    // Disable Autofill requesting access to AddressBook data. This will cause
    // the tests to hang on Mac.
    autofill::test::DisableSystemServices(browser()->profile()->GetPrefs());

    // Set observer for popup.
    ChromePasswordManagerClient* client =
        ChromePasswordManagerClient::FromWebContents(WebContents());
    client->SetTestObserver(&observer_);

    NavigateToFile("/password/signup_form.html");
  }

  void TearDownOnMainThread() override {
    PasswordManagerBrowserTestBase::TearDownOnMainThread();

    // Clean up UI.
    ChromePasswordManagerClient* client =
        ChromePasswordManagerClient::FromWebContents(WebContents());
    client->HidePasswordGenerationPopup();

    autofill::test::ReenableSystemServices();
  }

  std::string GetFieldValue(const std::string& field_id) {
    std::string value;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        RenderViewHost(),
        "window.domAutomationController.send("
        "    document.getElementById('" + field_id + "').value);",
        &value));
    return value;
  }

  std::string GetFocusedElement() {
    std::string focused_element;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        RenderViewHost(),
        "window.domAutomationController.send("
        "    document.activeElement.id)",
        &focused_element));
    return focused_element;
  }

  void FocusPasswordField() {
    ASSERT_TRUE(content::ExecuteScript(
        RenderViewHost(),
        "document.getElementById('password_field').focus()"));
  }

  void SendKeyToPopup(ui::KeyboardCode key) {
    content::NativeWebKeyboardEvent event(
        blink::WebKeyboardEvent::RawKeyDown, blink::WebInputEvent::NoModifiers,
        blink::WebInputEvent::TimeStampForTesting);
    event.windowsKeyCode = key;
    RenderViewHost()->GetWidget()->ForwardKeyboardEvent(event);
  }

  bool GenerationPopupShowing() {
    return observer_.popup_showing() && observer_.password_visible();
  }

  bool EditingPopupShowing() {
    return observer_.popup_showing() && !observer_.password_visible();
  }

 private:
  TestPopupObserver observer_;
};

// Disabled due to flakiness due to resizes, see http://crbug.com/407998.
IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       DISABLED_PopupShownAndPasswordSelected) {
  FocusPasswordField();
  EXPECT_TRUE(GenerationPopupShowing());
  SendKeyToPopup(ui::VKEY_DOWN);
  SendKeyToPopup(ui::VKEY_RETURN);

  // Selecting the password should fill the field and move focus to the
  // submit button.
  EXPECT_FALSE(GetFieldValue("password_field").empty());
  EXPECT_FALSE(GenerationPopupShowing());
  EXPECT_FALSE(EditingPopupShowing());
  EXPECT_EQ("input_submit_button", GetFocusedElement());

  // Re-focusing the password field should show the editing popup.
  FocusPasswordField();
  EXPECT_TRUE(EditingPopupShowing());
}

// Disabled due to flakiness due to resizes, see http://crbug.com/407998.
IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       DISABLED_PopupShownAndDismissed) {
  FocusPasswordField();
  EXPECT_TRUE(GenerationPopupShowing());

  SendKeyToPopup(ui::VKEY_ESCAPE);

  // Popup is dismissed.
  EXPECT_FALSE(GenerationPopupShowing());
}

// Disabled due to flakiness due to resizes, see http://crbug.com/407998.
IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       DISABLED_PopupShownAndDismissedByScrolling) {
  FocusPasswordField();
  EXPECT_TRUE(GenerationPopupShowing());

  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(),
                                     "window.scrollTo(100, 0);"));

  EXPECT_FALSE(GenerationPopupShowing());
}

// Disabled due to flakiness due to resizes, see http://crbug.com/407998.
IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       DISABLED_GenerationTriggeredInIFrame) {
  NavigateToFile("/password/framed_signup_form.html");

  std::string focus_script =
      "var frame = document.getElementById('signup_iframe');"
      "var frame_doc = frame.contentDocument;"
      "frame_doc.getElementById('password_field').focus();";

  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), focus_script));
  EXPECT_TRUE(GenerationPopupShowing());
}

// Disabled due to flakiness due to resizes, see http://crbug.com/407998.
IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       DISABLED_AutoSavingGeneratedPassword) {
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());

  FocusPasswordField();
  EXPECT_TRUE(GenerationPopupShowing());
  SendKeyToPopup(ui::VKEY_DOWN);
  SendKeyToPopup(ui::VKEY_RETURN);

  // Change username and submit.
  NavigationObserver observer(WebContents());
  std::string submit_script =
      "document.getElementById('username_field').value = 'something';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), submit_script));
  observer.Wait();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_FALSE(password_store->IsEmpty());

  // Make sure the username is correct.
  password_manager::TestPasswordStore::PasswordMap stored_passwords =
      password_store->stored_passwords();
  EXPECT_EQ(1u, stored_passwords.size());
  EXPECT_EQ(1u, stored_passwords.begin()->second.size());
  EXPECT_EQ(base::UTF8ToUTF16("something"),
            (stored_passwords.begin()->second)[0].username_value);
}
