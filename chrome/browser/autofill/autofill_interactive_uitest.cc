// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/browser/autofill_common_test.h"
#include "components/autofill/browser/autofill_external_delegate.h"
#include "components/autofill/browser/autofill_manager.h"
#include "components/autofill/browser/autofill_manager_test_delegate.h"
#include "components/autofill/browser/autofill_profile.h"
#include "components/autofill/browser/personal_data_manager.h"
#include "components/autofill/browser/personal_data_manager_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/base/keycodes/keyboard_codes.h"

using content::RenderViewHost;

// TODO(csharp): Most of this file was just copied from autofill_browsertests.cc
// The repeated code should be moved into a helper file, instead of being
// repeated.

namespace autofill {

static const char* kDataURIPrefix = "data:text/html;charset=utf-8,";
static const char* kTestFormString =
    "<form action=\"http://www.example.com/\" method=\"POST\">"
    "<label for=\"firstname\">First name:</label>"
    " <input type=\"text\" id=\"firstname\""
    "        onFocus=\"domAutomationController.send(true)\"><br>"
    "<label for=\"lastname\">Last name:</label>"
    " <input type=\"text\" id=\"lastname\"><br>"
    "<label for=\"address1\">Address line 1:</label>"
    " <input type=\"text\" id=\"address1\"><br>"
    "<label for=\"address2\">Address line 2:</label>"
    " <input type=\"text\" id=\"address2\"><br>"
    "<label for=\"city\">City:</label>"
    " <input type=\"text\" id=\"city\"><br>"
    "<label for=\"state\">State:</label>"
    " <select id=\"state\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">California</option>"
    " <option value=\"TX\">Texas</option>"
    " </select><br>"
    "<label for=\"zip\">ZIP code:</label>"
    " <input type=\"text\" id=\"zip\"><br>"
    "<label for=\"country\">Country:</label>"
    " <select id=\"country\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">Canada</option>"
    " <option value=\"US\">United States</option>"
    " </select><br>"
    "<label for=\"phone\">Phone number:</label>"
    " <input type=\"text\" id=\"phone\"><br>"
    "</form>";

class AutofillManagerTestDelegateImpl
    : public autofill::AutofillManagerTestDelegate {
 public:
  AutofillManagerTestDelegateImpl() {}

  virtual void DidPreviewFormData() OVERRIDE {
    loop_runner_->Quit();
  }

  virtual void DidFillFormData() OVERRIDE {
    loop_runner_->Quit();
  }

  virtual void DidShowSuggestions() OVERRIDE {
    loop_runner_->Quit();
  }

  void Reset() {
    loop_runner_ = new content::MessageLoopRunner();
  }

  void Wait() {
    loop_runner_->Run();
  }

 private:
  scoped_refptr<content::MessageLoopRunner> loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(AutofillManagerTestDelegateImpl);
};

class WindowedPersonalDataManagerObserver
    : public PersonalDataManagerObserver,
      public content::NotificationObserver {
 public:
  explicit WindowedPersonalDataManagerObserver(Browser* browser)
      : alerted_(false),
        has_run_message_loop_(false),
        browser_(browser),
        infobar_service_(NULL) {
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        AddObserver(this);
    registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
                   content::NotificationService::AllSources());
  }

  virtual ~WindowedPersonalDataManagerObserver() {
    if (infobar_service_ && infobar_service_->infobar_count() > 0)
      infobar_service_->RemoveInfoBar(infobar_service_->infobar_at(0));
  }

  void Wait() {
    if (!alerted_) {
      has_run_message_loop_ = true;
      content::RunMessageLoop();
    }
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        RemoveObserver(this);
  }

  // PersonalDataManagerObserver:
  virtual void OnPersonalDataChanged() OVERRIDE {
    if (has_run_message_loop_) {
      MessageLoopForUI::current()->Quit();
      has_run_message_loop_ = false;
    }
    alerted_ = true;
  }

  virtual void OnInsufficientFormData() OVERRIDE {
    OnPersonalDataChanged();
  }

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    // Accept in the infobar.
    infobar_service_ = InfoBarService::FromWebContents(
        browser_->tab_strip_model()->GetActiveWebContents());
    InfoBarDelegate* infobar = infobar_service_->infobar_at(0);

    ConfirmInfoBarDelegate* confirm_infobar =
        infobar->AsConfirmInfoBarDelegate();
    confirm_infobar->Accept();
  }

 private:
  bool alerted_;
  bool has_run_message_loop_;
  Browser* browser_;
  content::NotificationRegistrar registrar_;
  InfoBarService* infobar_service_;
};

class TestAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  TestAutofillExternalDelegate(content::WebContents* web_contents,
                               AutofillManager* autofill_manager)
      : AutofillExternalDelegate(web_contents, autofill_manager),
        keyboard_listener_(NULL) {
  }
  virtual ~TestAutofillExternalDelegate() {}

  virtual void OnPopupShown(content::KeyboardListener* listener) OVERRIDE {
    AutofillExternalDelegate::OnPopupShown(listener);
    keyboard_listener_ = listener;
  }

  virtual void OnPopupHidden(content::KeyboardListener* listener) OVERRIDE {
    keyboard_listener_ = NULL;
    AutofillExternalDelegate::OnPopupHidden(listener);
  }

  content::KeyboardListener* keyboard_listener() {
    return keyboard_listener_;
  }

 private:
  // The popup that is currently registered as a keyboard listener, or NULL if
  // there is none.
  content::KeyboardListener* keyboard_listener_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillExternalDelegate);
};

class AutofillInteractiveTest : public InProcessBrowserTest {
 protected:
  AutofillInteractiveTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    // Don't want Keychain coming up on Mac.
    test::DisableSystemServices(browser()->profile());

    // When testing the native UI, hook up a test external delegate, which
    // allows us to forward keyboard events to the popup directly.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    AutofillManager* autofill_manager =
        AutofillManager::FromWebContents(web_contents);
    if (autofill_manager->IsNativeUiEnabled()) {
      external_delegate_.reset(
          new TestAutofillExternalDelegate(web_contents, autofill_manager));
      autofill_manager->SetExternalDelegate(external_delegate_.get());
    }
    autofill_manager->SetTestDelegate(&test_delegate_);
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    // Make sure to close any showing popups prior to tearing down the UI.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    AutofillManager* autofill_manager =
        AutofillManager::FromWebContents(web_contents);
    autofill_manager->delegate()->HideAutofillPopup();
  }

  PersonalDataManager* personal_data_manager() {
    return PersonalDataManagerFactory::GetForProfile(browser()->profile());
  }

  void CreateTestProfile() {
    AutofillProfile profile;
    test::SetProfileInfo(
        &profile, "Milton", "C.", "Waddams",
        "red.swingline@initech.com", "Initech", "4120 Freidrich Lane",
        "Basement", "Austin", "Texas", "78744", "US", "5125551234");

    WindowedPersonalDataManagerObserver observer(browser());
    personal_data_manager()->AddProfile(profile);

    // AddProfile is asynchronous. Wait for it to finish before continuing the
    // tests.
    observer.Wait();
  }

  void ExpectFieldValue(const std::string& field_name,
                        const std::string& expected_value) {
    std::string value;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send("
        "    document.getElementById('" + field_name + "').value);",
        &value));
    EXPECT_EQ(expected_value, value);
  }

  RenderViewHost* render_view_host() {
    return browser()->tab_strip_model()->GetActiveWebContents()->
        GetRenderViewHost();
  }

  void FocusFirstNameField() {
    LOG(WARNING) << "Clicking on the tab.";
    content::SimulateMouseClick(
        browser()->tab_strip_model()->GetActiveWebContents(),
        0,
        WebKit::WebMouseEvent::ButtonLeft);

    LOG(WARNING) << "Focusing the first name field.";
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        render_view_host(),
        "if (document.readyState === 'complete')"
        "  document.getElementById('firstname').focus();"
        "else"
        "  domAutomationController.send(false);",
        &result));
    ASSERT_TRUE(result);
  }

  void ExpectFilledTestForm() {
    ExpectFieldValue("firstname", "Milton");
    ExpectFieldValue("lastname", "Waddams");
    ExpectFieldValue("address1", "4120 Freidrich Lane");
    ExpectFieldValue("address2", "Basement");
    ExpectFieldValue("city", "Austin");
    ExpectFieldValue("state", "TX");
    ExpectFieldValue("zip", "78744");
    ExpectFieldValue("country", "US");
    ExpectFieldValue("phone", "5125551234");
  }

  void SendKeyToPageAndWait(ui::KeyboardCode key) {
    test_delegate_.Reset();
    content::SimulateKeyPress(
        browser()->tab_strip_model()->GetActiveWebContents(),
        key, false, false, false, false);
    test_delegate_.Wait();
  }

  void SendKeyToPopupAndWait(ui::KeyboardCode key) {
    // TODO(isherman): Remove this condition once the WebKit popup UI code is
    // removed.
    if (!external_delegate_) {
      // When testing the WebKit-based UI, route all keys to the page.
      SendKeyToPageAndWait(key);
      return;
    }

    // When testing the native UI, route popup-targeted key presses via the
    // external delegate.
    content::NativeWebKeyboardEvent event;
    event.windowsKeyCode = key;
    test_delegate_.Reset();
    external_delegate_->keyboard_listener()->HandleKeyPressEvent(event);
    test_delegate_.Wait();
  }

  TestAutofillExternalDelegate* external_delegate() {
    return external_delegate_.get();
  }

  AutofillManagerTestDelegateImpl test_delegate_;

 private:
  scoped_ptr<TestAutofillExternalDelegate> external_delegate_;
};

IN_PROC_BROWSER_TEST_F(AutofillInteractiveTest, DISABLED_AutofillSelectViaTab) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Focus a fillable field.
  FocusFirstNameField();

  // Press the down arrow to initiate Autofill and wait for the popup to be
  // shown.
  SendKeyToPageAndWait(ui::VKEY_DOWN);

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyToPopupAndWait(ui::VKEY_DOWN);

  // Press tab to accept the autofill suggestions.
  SendKeyToPopupAndWait(ui::VKEY_TAB);

  // The form should be filled.
  ExpectFilledTestForm();
}

}  // namespace autofill
