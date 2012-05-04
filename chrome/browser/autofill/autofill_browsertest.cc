// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_observer.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/translate_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/test/mock_render_process_host.h"
#include "content/test/test_renderer_host.h"
#include "content/test/test_url_fetcher_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/keycodes/keyboard_codes.h"

using content::RenderViewHost;
using content::RenderViewHostTester;
using content::WebContents;

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

class WindowedPersonalDataManagerObserver : public PersonalDataManagerObserver {
 public:
  WindowedPersonalDataManagerObserver() :
      personal_data_changed_(false),
      has_run_message_loop_(false) {
  }

  void Wait() {
    if (!personal_data_changed_) {
      has_run_message_loop_ = true;
      ui_test_utils::RunMessageLoop();
    }
  }

  void OnPersonalDataChanged() OVERRIDE {
    if (has_run_message_loop_) {
      MessageLoopForUI::current()->Quit();
      has_run_message_loop_ = false;
    }

    personal_data_changed_ = true;
  }

 private:
  bool personal_data_changed_;
  bool has_run_message_loop_;
};

class AutofillTest : public InProcessBrowserTest {
 protected:
  AutofillTest() {
    EnableDOMAutomation();
  }

  void CreateTestProfile() {
    autofill_test::DisableSystemServices(browser()->profile());

    AutofillProfile profile;
    autofill_test::SetProfileInfo(
        &profile, "Milton", "C.", "Waddams",
        "red.swingline@initech.com", "Initech", "4120 Freidrich Lane",
        "Basement", "Austin", "Texas", "78744", "United States", "5125551234");

    PersonalDataManager* personal_data_manager =
        PersonalDataManagerFactory::GetForProfile(browser()->profile());
    ASSERT_TRUE(personal_data_manager);

    WindowedPersonalDataManagerObserver observer;
    personal_data_manager->SetObserver(&observer);

    personal_data_manager->AddProfile(profile);

    // AddProfile is asynchronous. Wait for it to finish before continuing the
    // tests.
    observer.Wait();
    personal_data_manager->RemoveObserver(&observer);
  }

  void ExpectFieldValue(const std::wstring& field_name,
                        const std::string& expected_value) {
    std::string value;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
        browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
        L"window.domAutomationController.send("
        L"document.getElementById('" + field_name + L"').value);", &value));
    EXPECT_EQ(expected_value, value);
  }

  RenderViewHost* render_view_host() {
    return browser()->GetSelectedWebContents()->GetRenderViewHost();
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

    fetcher->set_url(fetcher->GetOriginalURL());
    fetcher->set_status(status);
    fetcher->set_response_code(success ? 200 : 500);
    fetcher->SetResponseString(script);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  void FocusFirstNameField() {
    LOG(WARNING) << "Clicking on the tab.";
    ui_test_utils::SimulateMouseClick(browser()->GetSelectedWebContents());

    LOG(WARNING) << "Focusing the first name field.";
    bool result = false;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        render_view_host(), L"",
        L"if (document.readyState === 'complete')"
        L"  document.getElementById('firstname').focus();"
        L"else"
        L"  domAutomationController.send(false);",
        &result));
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

  void SendKeyAndWait(ui::KeyboardCode key, int notification_type) {
    ui_test_utils::WindowedNotificationObserver observer(
        notification_type, content::Source<RenderViewHost>(render_view_host()));
    ui_test_utils::SimulateKeyPress(
        browser()->GetSelectedWebContents(), key, false, false, false, false);
    observer.Wait();
  }

  void TryBasicFormFill() {
    FocusFirstNameField();

    // Start filling the first name field with "M" and wait for the popup to be
    // shown.
    LOG(WARNING) << "Typing 'M' to bring up the Autofill popup.";
    SendKeyAndWait(
        ui::VKEY_M, chrome::NOTIFICATION_AUTOFILL_DID_SHOW_SUGGESTIONS);

    // Press the down arrow to select the suggestion and preview the autofilled
    // form.
    LOG(WARNING) << "Simulating down arrow press to initiate Autofill preview.";
    SendKeyAndWait(
        ui::VKEY_DOWN, chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA);

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
    LOG(WARNING) << "Simulating Return press to fill the form.";
    SendKeyAndWait(
        ui::VKEY_RETURN, chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA);

    // The form should be filled.
    ExpectFilledTestForm();
  }

 private:
  TestURLFetcherFactory url_fetcher_factory_;
};

// Test that basic form fill is working.
IN_PROC_BROWSER_TEST_F(AutofillTest, BasicFormFill) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that form filling can be initiated by pressing the down arrow.
IN_PROC_BROWSER_TEST_F(AutofillTest, AutofillViaDownArrow) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Focus a fillable field.
  FocusFirstNameField();

  // Press the down arrow to initiate Autofill and wait for the popup to be
  // shown.
  SendKeyAndWait(
      ui::VKEY_DOWN, chrome::NOTIFICATION_AUTOFILL_DID_SHOW_SUGGESTIONS);

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyAndWait(
      ui::VKEY_DOWN, chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA);

  // Press Enter to accept the autofill suggestions.
  SendKeyAndWait(
      ui::VKEY_RETURN, chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA);

  // The form should be filled.
  ExpectFilledTestForm();
}

// Test that a JavaScript onchange event is fired after auto-filling a form.
IN_PROC_BROWSER_TEST_F(AutofillTest, OnChangeAfterAutofill) {
  CreateTestProfile();

  const char* kOnChangeScript =
      "<script>"
      "focused_fired = false;"
      "unfocused_fired = false;"
      "changed_select_fired = false;"
      "unchanged_select_fired = false;"
      "document.getElementById('firstname').onchange = function() {"
      "  focused_fired = true;"
      "};"
      "document.getElementById('lastname').onchange = function() {"
      "  unfocused_fired = true;"
      "};"
      "document.getElementById('state').onchange = function() {"
      "  changed_select_fired = true;"
      "};"
      "document.getElementById('country').onchange = function() {"
      "  unchanged_select_fired = true;"
      "};"
      "document.getElementById('country').value = 'US';"
      "</script>";

  // Load the test page.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString + kOnChangeScript)));

  // Invoke Autofill.
  FocusFirstNameField();

  // Start filling the first name field with "M" and wait for the popup to be
  // shown.
  SendKeyAndWait(
      ui::VKEY_M, chrome::NOTIFICATION_AUTOFILL_DID_SHOW_SUGGESTIONS);

  // Press the down arrow to select the suggestion and preview the autofilled
  // form.
  SendKeyAndWait(
      ui::VKEY_DOWN, chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA);

  // Press Enter to accept the autofill suggestions.
  SendKeyAndWait(
      ui::VKEY_RETURN, chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA);

  // The form should be filled.
  ExpectFilledTestForm();

  // The change event should have already fired for unfocused fields, both of
  // <input> and of <select> type. However, it should not yet have fired for the
  // focused field.
  bool focused_fired = false;
  bool unfocused_fired = false;
  bool changed_select_fired = false;
  bool unchanged_select_fired = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      render_view_host(), L"",
      L"domAutomationController.send(focused_fired);", &focused_fired));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      render_view_host(), L"",
      L"domAutomationController.send(unfocused_fired);", &unfocused_fired));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      render_view_host(), L"",
      L"domAutomationController.send(changed_select_fired);",
      &changed_select_fired));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      render_view_host(), L"",
      L"domAutomationController.send(unchanged_select_fired);",
      &unchanged_select_fired));
  EXPECT_FALSE(focused_fired);
  EXPECT_TRUE(unfocused_fired);
  EXPECT_TRUE(changed_select_fired);
  EXPECT_FALSE(unchanged_select_fired);

  // Unfocus the first name field. Its change event should fire.
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      render_view_host(), L"",
      L"document.getElementById('firstname').blur();"
      L"domAutomationController.send(focused_fired);", &focused_fired));
  EXPECT_TRUE(focused_fired);
}

// Test that we can autofill forms distinguished only by their |id| attribute.
IN_PROC_BROWSER_TEST_F(AutofillTest, AutofillFormsDistinguishedById) {
  CreateTestProfile();

  // Load the test page.
  const std::string kURL =
      std::string(kDataURIPrefix) + kTestFormString +
      "<script>"
      "var mainForm = document.forms[0];"
      "mainForm.id = 'mainForm';"
      "var newForm = document.createElement('form');"
      "newForm.action = mainForm.action;"
      "newForm.method = mainForm.method;"
      "newForm.id = 'newForm';"
      "mainForm.parentNode.insertBefore(newForm, mainForm);"
      "</script>";
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), GURL(kURL)));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that we properly autofill forms with repeated fields.
// In the wild, the repeated fields are typically either email fields
// (duplicated for "confirmation"); or variants that are hot-swapped via
// JavaScript, with only one actually visible at any given time.
IN_PROC_BROWSER_TEST_F(AutofillTest, AutofillFormWithRepeatedField) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) +
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
           "<label for=\"state_freeform\" style=\"display:none\">State:</label>"
           " <input type=\"text\" id=\"state_freeform\""
           "        style=\"display:none\"><br>"
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
           "</form>")));

  // Invoke Autofill.
  TryBasicFormFill();
  ExpectFieldValue(L"state_freeform", "");
}

#if defined(OS_WIN)
// Has been observed to fail on windows.  crbug.com/100062
#define MAYBE_AutofillFormWithNonAutofillableField \
    DISABLED_AutofillFormWithNonAutofillableField
#else
#define MAYBE_AutofillFormWithNonAutofillableField \
    AutofillFormWithNonAutofillableField
#endif

// Test that we properly autofill forms with non-autofillable fields.
IN_PROC_BROWSER_TEST_F(AutofillTest,
                       MAYBE_AutofillFormWithNonAutofillableField) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) +
           "<form action=\"http://www.example.com/\" method=\"POST\">"
           "<label for=\"firstname\">First name:</label>"
           " <input type=\"text\" id=\"firstname\""
           "        onFocus=\"domAutomationController.send(true)\"><br>"
           "<label for=\"middlename\">Middle name:</label>"
           " <input type=\"text\" id=\"middlename\" autocomplete=\"off\" /><br>"
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
           "</form>")));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that we can Autofill dynamically generated forms.
IN_PROC_BROWSER_TEST_F(AutofillTest, DynamicFormFill) {
  CreateTestProfile();

  // Load the test page.
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) +
           "<form id=\"form\" action=\"http://www.example.com/\""
           "      method=\"POST\"></form>"
           "<script>"
           "function AddElement(name, label) {"
           "  var form = document.getElementById('form');"
           ""
           "  var label_text = document.createTextNode(label);"
           "  var label_element = document.createElement('label');"
           "  label_element.setAttribute('for', name);"
           "  label_element.appendChild(label_text);"
           "  form.appendChild(label_element);"
           ""
           "  if (name === 'state' || name === 'country') {"
           "    var select_element = document.createElement('select');"
           "    select_element.setAttribute('id', name);"
           "    select_element.setAttribute('name', name);"
           ""
           "    /* Add an empty selected option. */"
           "    var default_option = new Option('--', '', true);"
           "    select_element.appendChild(default_option);"
           ""
           "    /* Add the other options. */"
           "    if (name == 'state') {"
           "      var option1 = new Option('California', 'CA');"
           "      select_element.appendChild(option1);"
           "      var option2 = new Option('Texas', 'TX');"
           "      select_element.appendChild(option2);"
           "    } else {"
           "      var option1 = new Option('Canada', 'CA');"
           "      select_element.appendChild(option1);"
           "      var option2 = new Option('United States', 'US');"
           "      select_element.appendChild(option2);"
           "    }"
           ""
           "    form.appendChild(select_element);"
           "  } else {"
           "    var input_element = document.createElement('input');"
           "    input_element.setAttribute('id', name);"
           "    input_element.setAttribute('name', name);"
           ""
           "    /* Add the onFocus listener to the 'firstname' field. */"
           "    if (name === 'firstname') {"
           "      input_element.setAttribute("
           "          'onFocus', 'domAutomationController.send(true)');"
           "    }"
           ""
           "    form.appendChild(input_element);"
           "  }"
           ""
           "  form.appendChild(document.createElement('br'));"
           "};"
           ""
           "function BuildForm() {"
           "  var elements = ["
           "    ['firstname', 'First name:'],"
           "    ['lastname', 'Last name:'],"
           "    ['address1', 'Address line 1:'],"
           "    ['address2', 'Address line 2:'],"
           "    ['city', 'City:'],"
           "    ['state', 'State:'],"
           "    ['zip', 'ZIP code:'],"
           "    ['country', 'Country:'],"
           "    ['phone', 'Phone number:'],"
           "  ];"
           ""
           "  for (var i = 0; i < elements.length; i++) {"
           "    var name = elements[i][0];"
           "    var label = elements[i][1];"
           "    AddElement(name, label);"
           "  }"
           "};"
           "</script>")));

  // Dynamically construct the form.
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(render_view_host(), L"",
                                               L"BuildForm();"));

  // Invoke Autofill.
  TryBasicFormFill();
}

// Test that form filling works after reloading the current page.
// This test brought to you by http://crbug.com/69204
#if defined(OS_MACOSX)
// Sometimes times out on Mac: http://crbug.com/81451
// Currently enabled for logging.
#define MAYBE_AutofillAfterReload AutofillAfterReload
#else
#define MAYBE_AutofillAfterReload AutofillAfterReload
#endif
IN_PROC_BROWSER_TEST_F(AutofillTest, MAYBE_AutofillAfterReload) {
  LOG(WARNING) << "Creating test profile.";
  CreateTestProfile();

  // Load the test page.
  LOG(WARNING) << "Bringing browser window to front.";
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  LOG(WARNING) << "Navigating to URL.";
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(),
      GURL(std::string(kDataURIPrefix) + kTestFormString)));

  // Reload the page.
  LOG(WARNING) << "Reloading the page.";
  WebContents* tab =
      browser()->GetSelectedTabContentsWrapper()->web_contents();
  tab->GetController().Reload(false);
  ui_test_utils::WaitForLoadStop(tab);

  // Invoke Autofill.
  LOG(WARNING) << "Trying to fill the form.";
  TryBasicFormFill();
}

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
// Test that autofill works after page translation.
// http://crbug.com/81451
IN_PROC_BROWSER_TEST_F(AutofillTest, DISABLED_AutofillAfterTranslate) {
#else
IN_PROC_BROWSER_TEST_F(AutofillTest, AutofillAfterTranslate) {
#endif
  CreateTestProfile();

  GURL url(std::string(kDataURIPrefix) +
               "<form action=\"http://www.example.com/\" method=\"POST\">"
               "<label for=\"fn\">なまえ</label>"
               " <input type=\"text\" id=\"fn\""
               "        onFocus=\"domAutomationController.send(true)\""
               "><br>"
               "<label for=\"ln\">みょうじ</label>"
               " <input type=\"text\" id=\"ln\"><br>"
               "<label for=\"a1\">Address line 1:</label>"
               " <input type=\"text\" id=\"a1\"><br>"
               "<label for=\"a2\">Address line 2:</label>"
               " <input type=\"text\" id=\"a2\"><br>"
               "<label for=\"ci\">City:</label>"
               " <input type=\"text\" id=\"ci\"><br>"
               "<label for=\"st\">State:</label>"
               " <select id=\"st\">"
               " <option value=\"\" selected=\"yes\">--</option>"
               " <option value=\"CA\">California</option>"
               " <option value=\"TX\">Texas</option>"
               " </select><br>"
               "<label for=\"z\">ZIP code:</label>"
               " <input type=\"text\" id=\"z\"><br>"
               "<label for=\"co\">Country:</label>"
               " <select id=\"co\">"
               " <option value=\"\" selected=\"yes\">--</option>"
               " <option value=\"CA\">Canada</option>"
               " <option value=\"US\">United States</option>"
               " </select><br>"
               "<label for=\"ph\">Phone number:</label>"
               " <input type=\"text\" id=\"ph\"><br>"
               "</form>");
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_NO_FATAL_FAILURE(ui_test_utils::NavigateToURL(browser(), url));

  // Get translation bar.
  RenderViewHostTester::TestOnMessageReceived(
      render_view_host(),
      ChromeViewHostMsg_TranslateLanguageDetermined(0, "ja", true));
  TranslateInfoBarDelegate* infobar =
      browser()->GetSelectedTabContentsWrapper()->infobar_tab_helper()->
          GetInfoBarDelegateAt(0)->AsTranslateInfoBarDelegate();

  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, infobar->type());

  // Simulate translation button press.
  infobar->Translate();

  // Simulate the translate script being retrieved.
  // Pass fake google.translate lib as the translate script.
  SimulateURLFetch(true);

  ui_test_utils::WindowedNotificationObserver translation_observer(
      chrome::NOTIFICATION_PAGE_TRANSLATED,
      content::NotificationService::AllSources());

  // Simulate translation to kick onTranslateElementLoad.
  // But right now, the call stucks here.
  // Once click the text field, it starts again.
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
      render_view_host(), L"",
      L"cr.googleTranslate.onTranslateElementLoad();"));

  // Simulate the render notifying the translation has been done.
  translation_observer.Wait();

  TryBasicFormFill();
}
