// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/password_manager/password_generation_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/password_generation_popup_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

class TestPopupObserver : public autofill::PasswordGenerationPopupObserver {
 public:
  TestPopupObserver()
      : popup_showing_(false) {}
  virtual ~TestPopupObserver() {}

  virtual void OnPopupShown() OVERRIDE {
    popup_showing_ = true;
  }

  virtual void OnPopupHidden() OVERRIDE {
    popup_showing_ = false;
  }

  bool popup_showing() { return popup_showing_; }

 private:
  bool popup_showing_;
};

}  // namespace

class PasswordGenerationInteractiveTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Make sure the feature is enabled.
    command_line->AppendSwitch(autofill::switches::kEnablePasswordGeneration);

    // Don't require ping from autofill or blacklist checking.
    command_line->AppendSwitch(
        autofill::switches::kLocalHeuristicsOnlyForPasswordGeneration);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    // Disable Autofill requesting access to AddressBook data. This will cause
    // test tests to hang on Mac.
    autofill::test::DisableSystemServices(browser()->profile());

    // Set observer for popup.
    PasswordGenerationManager* generation_manager =
        PasswordGenerationManager::FromWebContents(GetWebContents());
    generation_manager->SetTestObserver(&observer_);

    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    GURL url = embedded_test_server()->GetURL("/password/signup_form.html");
    ui_test_utils::NavigateToURL(browser(), url);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // Cleanup UI.
    PasswordGenerationManager* generation_manager =
        PasswordGenerationManager::FromWebContents(GetWebContents());
    generation_manager->HidePopup();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::RenderViewHost* GetRenderViewHost() {
    return GetWebContents()->GetRenderViewHost();
  }

  std::string GetFieldValue(const std::string& field_id) {
    std::string value;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        GetRenderViewHost(),
        "window.domAutomationController.send("
        "    document.getElementById('" + field_id + "').value);",
        &value));
    return value;
  }

  void FocusPasswordField() {
    ASSERT_TRUE(content::ExecuteScript(
        GetRenderViewHost(),
        "document.getElementById('password_field').focus()"));
  }

  void SendKeyToPopup(ui::KeyboardCode key) {
    content::NativeWebKeyboardEvent event;
    event.windowsKeyCode = key;
    event.type = blink::WebKeyboardEvent::RawKeyDown;
    GetRenderViewHost()->ForwardKeyboardEvent(event);
  }

  bool popup_showing() {
    return observer_.popup_showing();
  }

 private:
  TestPopupObserver observer_;
};

#if defined(USE_AURA)
// Enabled on these platforms
#define MAYBE_PopupShownAndPasswordSelected PopupShownAndPasswordSelected
#define MAYBE_PopupShownAndDismissed PopupShownAndDismissed
#else
// Popup not enabled for these platforms yet.
#define MAYBE_PopupShownAndPasswordSelected DISABLED_PopupShownAndPasswordSelected
#define MAYBE_PopupShownAndDismissed DISABLED_PopupShownAndDismissed
#endif

IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       MAYBE_PopupShownAndPasswordSelected) {
  FocusPasswordField();
  EXPECT_TRUE(popup_showing());
  SendKeyToPopup(ui::VKEY_DOWN);
  SendKeyToPopup(ui::VKEY_RETURN);

  EXPECT_FALSE(GetFieldValue("password_field").empty());
}

IN_PROC_BROWSER_TEST_F(PasswordGenerationInteractiveTest,
                       MAYBE_PopupShownAndDismissed) {
  FocusPasswordField();
  EXPECT_TRUE(popup_showing());

  SendKeyToPopup(ui::VKEY_ESCAPE);

  // Popup is dismissed.
  EXPECT_FALSE(popup_showing());
}
