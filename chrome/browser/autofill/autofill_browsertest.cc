// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "app/keyboard_code_conversion.h"
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
#include "chrome/common/net/test_url_fetcher_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/translate_helper.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

static const char* kTestFormString =
    "<form action=\"http://www.google.com/\" method=\"POST\">"
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

  void SetUpProfile() {
    autofill_test::DisableSystemServices(browser()->profile());

    AutoFillProfile profile;
    autofill_test::SetProfileInfo(
        &profile, "Office Space", "Milton", "C.", "Waddams",
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

  RenderViewHost* rvh() {
    return browser()->GetSelectedTabContents()->render_view_host();
  }

  virtual void SetUp() {
    URLFetcher::set_factory(&url_fetcher_factory_);
    InProcessBrowserTest::SetUp();
  }

  void SimulateURLFetch(bool success) {
    TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    ASSERT_TRUE(fetcher);
    URLRequestStatus status;
    status.set_status(success ? URLRequestStatus::SUCCESS :
        URLRequestStatus::FAILED);

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

  void TryBasicFormFillWithMKey() {
    ASSERT_NO_FATAL_FAILURE(ui_test_utils::ClickOnView(browser(),
                                                       VIEW_ID_TAB_CONTAINER));
    ASSERT_TRUE(ui_test_utils::IsViewFocused(browser(),
                                             VIEW_ID_TAB_CONTAINER_FOCUS_VIEW));

    bool result = false;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        rvh(), L"", L"document.getElementById('firstname').focus();", &result));
    ASSERT_TRUE(result);
    // Start filling the first name field with "M" and wait for the popup to be
    // shown.
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), app::VKEY_M, false, true, false, false,
        NotificationType::AUTOFILL_DID_SHOW_SUGGESTIONS,
        Source<RenderViewHost>(rvh())));

    // Press the down arrow to select the suggestion and preview the autofilled
    // form.
    ASSERT_TRUE(ui_test_utils::SendKeyPressAndWait(
        browser(), app::VKEY_DOWN, false, false, false, false,
        NotificationType::AUTOFILL_DID_FILL_FORM_DATA,
        Source<RenderViewHost>(rvh())));

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
        browser(), app::VKEY_RETURN, false, false, false, false,
        NotificationType::AUTOFILL_DID_FILL_FORM_DATA,
        Source<RenderViewHost>(rvh())));

    // The form should be filled.
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

 private:
  TestURLFetcherFactory url_fetcher_factory_;
};

// Test that basic form fill is working.
IN_PROC_BROWSER_TEST_F(AutoFillTest, BasicFormFill) {
  SetUpProfile();

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL("data:text/html;charset=utf-8," + std::string(kTestFormString))));

  TryBasicFormFillWithMKey();
}

// Test that basic form fill is working.
IN_PROC_BROWSER_TEST_F(AutoFillTest, TranslateAndFormFill) {
  SetUpProfile();

  GURL url("data:text/html;charset=utf-8,"
              "<form action=\"http://www.google.com/\" method=\"POST\">"
              "<label for=\"firstname\">なまえ</label>"
              " <input type=\"text\" id=\"firstname\""
              "        onFocus=\"domAutomationController.send(true)\""
              " /><br />"
              "<label for=\"lastname\">みょうじ</label>"
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
              "</form>");
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(
      browser(), url));

  // Get translation bar.
  int page_id = browser()->GetSelectedTabContents()->controller().
      GetLastCommittedEntry()->page_id();
  rvh()->OnMessageReceived(ViewHostMsg_PageContents(0, url, page_id,
      UTF8ToUTF16("test"), "ja", true));
  TranslateInfoBarDelegate* infobar = browser()->GetSelectedTabContents()->
      GetInfoBarDelegateAt(0)->AsTranslateInfoBarDelegate();

  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, infobar->type());

  // Simulate press translation button.
  infobar->Translate();

  // Simulate the translate script being retrieved.
  // Pass fake google.translate lib as the translate script.
  SimulateURLFetch(true);

  // Simulate translation to kick onTranslateElementLoad.
  // But right now, the call stucks here.
  // Once click the text field, it starts again.
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      rvh(), L"", L"cr.googleTranslate.onTranslateElementLoad();"));

  // Simulate the render notifying the translation has been done.
  ui_test_utils::WaitForNotification(NotificationType::PAGE_TRANSLATED);

  TryBasicFormFillWithMKey();
}
