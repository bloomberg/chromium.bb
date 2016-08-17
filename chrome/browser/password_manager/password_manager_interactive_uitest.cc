// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/browser.h"
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

// TODO(crbug.com/616627): Flaky on Mac, CrOS and Linux.
#if defined(OS_MACOSX) || defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_UsernameChanged DISABLED_UsernameChanged
#else
#define MAYBE_UsernameChanged UsernameChanged
#endif
IN_PROC_BROWSER_TEST_F(PasswordManagerBrowserTestBase, MAYBE_UsernameChanged) {
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          PasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS).get());

  NavigateToFile("/password/signup_form.html");

  NavigationObserver observer(WebContents());
  std::unique_ptr<BubbleObserver> prompt_observer(
      new BubbleObserver(WebContents()));
  std::string fill_and_submit =
      "document.getElementById('username_field').value = 'temp';"
      "document.getElementById('password_field').value = 'random';"
      "document.getElementById('input_submit_button').click()";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  observer.Wait();
  EXPECT_TRUE(prompt_observer->IsShowingSavePrompt());
  prompt_observer->AcceptSavePrompt();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_FALSE(password_store->IsEmpty());

  // Reload the original page to have the saved credentials autofilled.
  NavigationObserver reload_observer(WebContents());
  NavigateToFile("/password/signup_form.html");
  reload_observer.Wait();

  // Let the user interact with the page, so that DOM gets modification events,
  // needed for autofilling fields.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::Left, gfx::Point(1, 1));

  WaitForElementValue("username_field", "temp");
  WaitForElementValue("password_field", "random");

  // Change username and submit. This should add the characters "ORARY" to the
  // already autofilled username.
  SimulateUserTypingInField(RenderViewHost(), WebContents(), "username_field");
  // TODO(gcasto): Not sure why this click is required.
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::Left, gfx::Point(1, 1));
  WaitForElementValue("username_field", "tempORARY");

  NavigationObserver second_observer(WebContents());
  std::unique_ptr<BubbleObserver> second_prompt_observer(
      new BubbleObserver(WebContents()));
  std::string submit =
      "document.getElementById('input_submit_button').click();";
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), submit));
  second_observer.Wait();
  EXPECT_TRUE(second_prompt_observer->IsShowingSavePrompt());
  second_prompt_observer->AcceptSavePrompt();

  // Spin the message loop to make sure the password store had a chance to save
  // the password.
  base::RunLoop third_run_loop;
  third_run_loop.RunUntilIdle();
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

}  // namespace password_manager
