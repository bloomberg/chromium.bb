// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

void SimulateUserTypingInField(content::RenderViewHost* render_view_host,
                               content::WebContents* web_contents,
                               const std::string& field_id) {
  std::string focus("document.getElementById('" + field_id + "').focus();");
  ASSERT_TRUE(content::ExecuteScript(render_view_host, focus));

  content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('O'),
                            ui::DomCode::US_O, ui::VKEY_O, false, false, false,
                            false);
  content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('R'),
                            ui::DomCode::US_R, ui::VKEY_R, false, false, false,
                            false);
  content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('A'),
                            ui::DomCode::US_A, ui::VKEY_A, false, false, false,
                            false);
  content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('R'),
                            ui::DomCode::US_R, ui::VKEY_R, false, false, false,
                            false);
  content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('Y'),
                            ui::DomCode::US_Y, ui::VKEY_Y, false, false, false,
                            false);
}

}  // namespace

namespace password_manager {

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTestBase, UsernameChanged) {
  // At first let us save a credential to the password store.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());
  autofill::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.origin = embedded_test_server()->base_url();
  signin_form.username_value = base::ASCIIToUTF16("temp");
  signin_form.password_value = base::ASCIIToUTF16("random");
  password_store->AddLogin(signin_form);

  // Load the page to have the saved credentials autofilled.
  NavigateToFile("/password/signup_form.html");

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));

  WaitForElementValue("username_field", "temp");
  WaitForElementValue("password_field", "random");

  // Change username and submit. This should add the characters "ORARY" to the
  // already autofilled username.
  SimulateUserTypingInField(RenderViewHost(), WebContents(), "username_field");

  // Move the focus out of the inputs before waiting because WaitForElementValue
  // uses "onchange" event. The event is triggered only when the control looses
  // focus.
  chrome::FocusLocationBar(browser());
  WaitForElementValue("username_field", "tempORARY");

  NavigationObserver navigation_observer(WebContents());
  BubbleObserver prompt_observer(WebContents());
  std::string submit =
      "document.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), submit));
  navigation_observer.Wait();
  EXPECT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  WaitForPasswordStore();
  EXPECT_FALSE(password_store->IsEmpty());

  // Verify that there are two saved password, the old password and the new
  // password.
  password_manager::TestPasswordStore::PasswordMap stored_passwords =
      password_store->stored_passwords();
  EXPECT_EQ(1u, stored_passwords.size());
  EXPECT_EQ(2u, stored_passwords.begin()->second.size());
  EXPECT_EQ(base::UTF8ToUTF16("temp"),
            (stored_passwords.begin()->second)[0].username_value);
  EXPECT_EQ(base::UTF8ToUTF16("tempORARY"),
            (stored_passwords.begin()->second)[1].username_value);
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTestBase,
                       ManualFallbackForSaving) {
  NavigateToFile("/password/password_form.html");

  std::string focus("document.getElementById('password_field').focus();");
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), focus));
  SimulateUserTypingInField(RenderViewHost(), WebContents(), "password_field");
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForFallbackForSaving();

  // The save prompt should be available but shouldn't pop up automatically.
  EXPECT_TRUE(prompt_observer.IsSavePromptAvailable());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());

  // Simulate several navigations.
  NavigateToFile("/password/signup_form.html");
  NavigateToFile("/password/failed.html");
  NavigateToFile("/password/done.html");

  // The save prompt should be still available.
  EXPECT_TRUE(prompt_observer.IsSavePromptAvailable());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();

  WaitForPasswordStore();
  CheckThatCredentialsStored(base::string16(), base::ASCIIToUTF16("ORARY"));
}

IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTestBase,
                       ManualFallbackForSaving_HideAfterTimeout) {
  NavigateToFile("/password/password_form.html");
  ManagePasswordsUIController::set_save_fallback_timeout_in_seconds(0);

  std::string focus("document.getElementById('password_field').focus();");
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), focus));
  SimulateUserTypingInField(RenderViewHost(), WebContents(), "password_field");
  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForFallbackForSaving();

  // Since the timeout is changed to zero for testing, the save prompt should be
  // hidden right after show.
  content::RunAllPendingInMessageLoop();
  EXPECT_FALSE(prompt_observer.IsSavePromptAvailable());
}

}  // namespace password_manager
