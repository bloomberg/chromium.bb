// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/mock_render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/translate_helper.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/keycodes/keyboard_codes.h"

static const char* kDataURIPrefix = "data:text/html;charset=utf-8,";
static const char* kTestFormString =
    "<form action=\"http://www.example.com/\" method=\"POST\">"
    "<label for=\"firstname\">First name:</label>"
    " <input type=\"text\" id=\"firstname\""
    "        onFocus=\"domAutomationController.send(true)\""
    " /><br />"
    "<label for=\"lastname\">Last name:</label>"
    " <input type=\"text\" id=\"lastname\" /><br />"
    "<label for=\"address1\">Address line 1:</label>"
    " <input type=\"text\" id=\"address1\" /><br />"
    "<label for=\"address2\">Address line 2:</label>"
    " <input type=\"text\" id=\"address2\" /><br />"
    "<label for=\"city\">City:</label>"
    " <input type=\"text\" id=\"city\" /><br />"
    "<label for=\"state\">State:</label>"
    " <select id=\"state\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">California</option>"
    " <option value=\"TX\">Texas</option>"
    " </select><br />"
    "<label for=\"zip\">ZIP code:</label>"
    " <input type=\"text\" id=\"zip\" /><br />"
    "<label for=\"country\">Country:</label>"
    " <select id=\"country\">"
    " <option value=\"\" selected=\"yes\">--</option>"
    " <option value=\"CA\">Canada</option>"
    " <option value=\"US\">United States</option>"
    " </select><br />"
    "<label for=\"phone\">Phone number:</label>"
    " <input type=\"text\" id=\"phone\" /><br />"
    "</form>";

class AutoFillTest : public InProcessBrowserTest {
 protected:
  AutoFillTest() {
    set_show_window(true);
    EnableDOMAutomation();
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    URLFetcher::set_factory(&url_fetcher_factory_);
  }

  void CreateTestProfile() {
    autofill_test::DisableSystemServices(browser()->profile());

    AutoFillProfile profile;
    autofill_test::SetProfileInfo(
        &profile, "Milton", "C.", "Waddams",
        "red.swingline@initech.com", "Initech", "4120 Freidrich Lane",
        "Basement", "Austin", "Texas", "78744", "United States", "5125551234",
        "5125550000");

    PersonalDataManager* personal_data_manager =
        browser()->profile()->GetPersonalDataManager();
    ASSERT_TRUE(personal_data_manager);

    std::vector<AutoFillProfile> profiles(1, profile);
    personal_data_manager->SetProfiles(&profiles);
  }

  void ExpectFieldValue(const std::wstring& field_name,
                        const std::string& expected_value) {
    std::string value;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
        browser()->GetSelectedTabContents()->render_view_host(), L"",
        L"window.domAutomationController.send("
        L"document.getElementById('" + field_name + L"').value);", &value));
    EXPECT_EQ(expected_value, value);
  }

  RenderViewHost* render_view_host() {
    return browser()->GetSelectedTabContents()->render_view_host();
  }

  void SimulateURLFetch(bool success) {
    TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    net::URLRequestStatus status;
    status.set_status(success ? net::URLRequestStatus::SUCCESS :
                                net::URLRequestStatus::FAILED);

    std::string script = " var google = {};"
        "google.translate = (function() {"
        "  return {"
        "    TranslateService: function() {"
        "      return {"
        "        isAvailable : function() {"
        "          return true;"
        "        },"
        "        restore : function() {"
        "          return;"
        "        },"
        "        getDetectedLanguage : function() {"
        "          return \"ja\";"
        "        },"
        "        translatePage : function(originalLang, targetLang,"
        "                                 onTranslateProgress) {"
        "          document.getElementsByTagName(\"body\")[0].innerHTML = '" +
        std::string(kTestFormString) +
        "              ';"
        "          onTranslateProgress(100, true, false);"
        "        }"
        "      };"
        "    }"
        "  };"
        "})();";

    fetcher->delegate()->OnURLFetchComplete(fetcher, fetcher->original_url(),
        status, success ? 200 : 500,
        ResponseCookies(),
        script);
  }

  void FocusFirstNameField() {
    ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                       VIEW_ID_TAB_CONTAINER));
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                             VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

    bool result = false;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        render_view_host(), L"",
        L"document.getElementById('firstname').focus();", &result));
    ASSERT_TRUE(result);
  }

  void ExpectFilledTestForm() {
    ExpectFieldValue(L"firstname", "Milton");
    ExpectFieldValue(L"lastname", "Waddams");
    ExpectFieldValue(L"address1", "4120 Freidrich Lane");
    ExpectFieldValue(L"address2", "Basement");
    ExpectFieldValue(L"city", "Austin");
    ExpectFieldValue(L"state", "TX");
    ExpectFieldValue(L"zip", "78744");
    ExpectFieldValue(L"country", "US");
    ExpectFieldValue(L"phone", "5125551234");
  }

  void TryBasicFormFill() {
    FocusFirstNameField();

    // Start filling the first name field with "M" and wait for the popup to be
    // shown.
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_M, false, true, false, false,
        NotificationType::AUTOFILL_DID_SHOW_SUGGESTIONS,
        Source<RenderViewHost>(render_view_host())));

    // Press the down arrow to select the suggestion and preview the autofilled
    // form.
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_DOWN, false, false, false, false,
        NotificationType::AUTOFILL_DID_FILL_FORM_DATA,
        Source<RenderViewHost>(render_view_host())));

    // The previewed values should not be accessible to JavaScript.
    ExpectFieldValue(L"firstname", "M");
    ExpectFieldValue(L"lastname", "");
    ExpectFieldValue(L"address1", "");
    ExpectFieldValue(L"address2", "");
    ExpectFieldValue(L"city", "");
    ExpectFieldValue(L"state", "");
    ExpectFieldValue(L"zip", "");
    ExpectFieldValue(L"country", "");
    ExpectFieldValue(L"phone", "");
    // TODO(isherman): It would be nice to test that the previewed values are
    // displayed: http://crbug.com/57220

    // Press Enter to accept the autofill suggestions.
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), ui::VKEY_RETURN, false, false, false, false,
        NotificationType::AUTOFILL_DID_FILL_FORM_DATA,
        Source<RenderViewHost>(render_view_host())));

    // The form should be filled.
    ExpectFilledTestForm();
  }

 private:
  TestURLFetcherFactory url_fetcher_factory_;
};

// Test that basic form fill is working.
IN_PROC_BROWSER_TEST_F(AutoFillTest, BasicFormFill) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Invoke AutoFill.
  TryBasicFormFill();
}

// Test that form filling can be initiated by pressing the down arrow.
IN_PROC_BROWSER_TEST_F(AutoFillTest, AutoFillViaDownArrow) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Focus a fillable field.
  FocusFirstNameField();

  // Press the down arrow to initiate AutoFill and wait for the popup to be
  // shown.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_DOWN, false, false, false, false,
      NotificationType::AUTOFILL_DID_SHOW_SUGGESTIONS,
      Source<RenderViewHost>(render_view_host())));

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_DOWN, false, false, false, false,
      NotificationType::AUTOFILL_DID_FILL_FORM_DATA,
      Source<RenderViewHost>(render_view_host())));

  // Press Enter to accept the autofill suggestions.
  ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
      browser(), ui::VKEY_RETURN, false, false, false, false,
      NotificationType::AUTOFILL_DID_FILL_FORM_DATA,
      Source<RenderViewHost>(render_view_host())));

  // The form should be filled.
  ExpectFilledTestForm();
}

// Test that form filling works after reloading the current page.
// This test brought to you by http://crbug.com/69204
IN_PROC_BROWSER_TEST_F(AutoFillTest, AutoFillAfterReload) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Reload the page.
  NavigationController* controller =
      &browser()->GetSelectedTabContentsWrapper()->tab_contents()->controller();
  controller->Reload(false);
  ui_test_utils::WaitForLoadStop(controller);

  // Invoke AutoFill.
  TryBasicFormFill();
}

// Test that autofill works after page translation.
IN_PROC_BROWSER_TEST_F(AutoFillTest, AutoFillAfterTranslate) {
  CreateTestProfile();

  GURL url(std::string(kDataURIPrefix) +
               "<form action=\"http://www.example.com/\" method=\"POST\">"
               "<label for=\"fn\">なまえ</label>"
               " <input type=\"text\" id=\"fn\""
               "        onFocus=\"domAutomationController.send(true)\""
               " /><br />"
               "<label for=\"ln\">みょうじ</label>"
               " <input type=\"text\" id=\"ln\" /><br />"
               "<label for=\"a1\">Address line 1:</label>"
               " <input type=\"text\" id=\"a1\" /><br />"
               "<label for=\"a2\">Address line 2:</label>"
               " <input type=\"text\" id=\"a2\" /><br />"
               "<label for=\"ci\">City:</label>"
               " <input type=\"text\" id=\"ci\" /><br />"
               "<label for=\"st\">State:</label>"
               " <select id=\"st\">"
               " <option value=\"\" selected=\"yes\">--</option>"
               " <option value=\"CA\">California</option>"
               " <option value=\"TX\">Texas</option>"
               " </select><br />"
               "<label for=\"z\">ZIP code:</label>"
               " <input type=\"text\" id=\"z\" /><br />"
               "<label for=\"co\">Country:</label>"
               " <select id=\"co\">"
               " <option value=\"\" selected=\"yes\">--</option>"
               " <option value=\"CA\">Canada</option>"
               " <option value=\"US\">United States</option>"
               " </select><br />"
               "<label for=\"ph\">Phone number:</label>"
               " <input type=\"text\" id=\"ph\" /><br />"
               "</form>");
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  // Get translation bar.
  int page_id = browser()->GetSelectedTabContents()->controller().
      GetLastCommittedEntry()->page_id();
  render_view_host()->OnMessageReceived(ViewHostMsg_PageContents(
      0, url, page_id, ASCIIToUTF16("test"), "ja", true));
  TranslateInfoBarDelegate* infobar = browser()->GetSelectedTabContents()->
      GetInfoBarDelegateAt(0)->AsTranslateInfoBarDelegate();

  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, infobar->type());

  // Simulate translation button press.
  infobar->Translate();

  // Simulate the translate script being retrieved.
  // Pass fake google.translate lib as the translate script.
  SimulateURLFetch(true);

  // Simulate translation to kick onTranslateElementLoad.
  // But right now, the call stucks here.
  // Once click the text field, it starts again.
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      render_view_host(), L"",
      L"cr.googleTranslate.onTranslateElementLoad();"));

  // Simulate the render notifying the translation has been done.
  ui_test_utils::WaitForNotification(NotificationType::PAGE_TRANSLATED);

  TryBasicFormFill();
}
