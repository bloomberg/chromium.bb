// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/personal_data_manager_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/translate/translate_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "net/url_request/test_url_fetcher_factory.h"
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

class WindowedPersonalDataManagerObserver
    : public PersonalDataManagerObserver,
      public content::NotificationObserver {
 public:
  explicit WindowedPersonalDataManagerObserver(Browser* browser) :
      alerted_(false),
      has_run_message_loop_(false),
      browser_(browser),
      infobar_service_(NULL) {
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        SetObserver(this);
    registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
                   content::NotificationService::AllSources());
  }

  ~WindowedPersonalDataManagerObserver() {
    if (!infobar_service_)
      return;

    InfoBarDelegate* infobar = NULL;
    if (infobar_service_->GetInfoBarCount() > 0 &&
        (infobar = infobar_service_->GetInfoBarDelegateAt(0))) {
      infobar_service_->RemoveInfoBar(infobar);
    }
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
    infobar_service_ =
        InfoBarService::ForTab(chrome::GetActiveTabContents(browser_));
    InfoBarDelegate* infobar = infobar_service_->GetInfoBarDelegateAt(0);

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

class AutofillTest : public InProcessBrowserTest {
 protected:
  AutofillTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    // Don't want Keychain coming up on Mac.
    autofill_test::DisableSystemServices(browser()->profile());
  }

  PersonalDataManager* personal_data_manager() {
    return PersonalDataManagerFactory::GetForProfile(browser()->profile());
  }

  void CreateTestProfile() {
    AutofillProfile profile;
    autofill_test::SetProfileInfo(
        &profile, "Milton", "C.", "Waddams",
        "red.swingline@initech.com", "Initech", "4120 Freidrich Lane",
        "Basement", "Austin", "Texas", "78744", "United States", "5125551234");

    WindowedPersonalDataManagerObserver observer(browser());
    personal_data_manager()->AddProfile(profile);

    // AddProfile is asynchronous. Wait for it to finish before continuing the
    // tests.
    observer.Wait();
  }

  void SetProfiles(std::vector<AutofillProfile>* profiles) {
    WindowedPersonalDataManagerObserver observer(browser());
    personal_data_manager()->SetProfiles(profiles);
    observer.Wait();
  }

  void SetProfile(const AutofillProfile& profile) {
    std::vector<AutofillProfile> profiles;
    profiles.push_back(profile);
    SetProfiles(&profiles);
  }

  void SetCards(std::vector<CreditCard>* cards) {
    WindowedPersonalDataManagerObserver observer(browser());
    personal_data_manager()->SetCreditCards(cards);
    observer.Wait();
  }

  void SetCard(const CreditCard& card) {
    std::vector<CreditCard> cards;
    cards.push_back(card);
    SetCards(&cards);
  }

  typedef std::map<std::string, std::string> FormMap;
  // Navigate to the form, input values into the fields, and submit the form.
  // The function returns after the PersonalDataManager is updated.
  void FillFormAndSubmit(const std::string& filename, const FormMap& data) {
    GURL url = test_server()->GetURL("files/autofill/" + filename);
    ui_test_utils::NavigateToURL(browser(), url);

    std::string js;
    for (FormMap::const_iterator i = data.begin(); i != data.end(); ++i) {
      js += "document.getElementById('" + i->first + "').value = '" +
            i->second + "';";
    }
    js += "document.getElementById('testform').submit();";

    WindowedPersonalDataManagerObserver observer(browser());
    ASSERT_TRUE(
        content::ExecuteJavaScript(render_view_host(), L"", ASCIIToWide(js)));
    observer.Wait();
  }

  void SubmitCreditCard(const char* name,
                        const char* number,
                        const char* exp_month,
                        const char* exp_year) {
    FormMap data;
    data["CREDIT_CARD_NAME"] = name;
    data["CREDIT_CARD_NUMBER"] = number;
    data["CREDIT_CARD_EXP_MONTH"] = exp_month;
    data["CREDIT_CARD_EXP_4_DIGIT_YEAR"] = exp_year;
    FillFormAndSubmit("autofill_creditcard_form.html", data);
  }

  // Populates a webpage form using autofill data and keypress events.
  // This function focuses the specified input field in the form, and then
  // sends keypress events to the tab to cause the form to be populated.
  void PopulateForm(const std::string& field_id) {
    std::string js("document.getElementById('" + field_id + "').focus();");
    ASSERT_TRUE(
        content::ExecuteJavaScript(render_view_host(), L"", ASCIIToWide(js)));

    SendKeyAndWait(ui::VKEY_DOWN,
                   chrome::NOTIFICATION_AUTOFILL_DID_SHOW_SUGGESTIONS);
    SendKeyAndWait(ui::VKEY_DOWN,
                   chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA);
    SendKeyAndWait(ui::VKEY_RETURN,
                   chrome::NOTIFICATION_AUTOFILL_DID_FILL_FORM_DATA);
  }

  // Aggregate profiles from forms into Autofill preferences. Returns the number
  // of parsed profiles.
  int AggregateProfilesIntoAutofillPrefs(const std::string& filename) {
    CHECK(test_server()->Start());

    std::string data;
    FilePath data_file = ui_test_utils::GetTestFilePath(
        FilePath().AppendASCII("autofill"), FilePath().AppendASCII(filename));
    CHECK(file_util::ReadFileToString(data_file, &data));
    std::vector<std::string> lines;
    base::SplitString(data, '\n', &lines);
    for (size_t i = 0; i < lines.size(); ++i) {
      if (StartsWithASCII(lines[i], "#", false))
        continue;
      std::vector<std::string> fields;
      base::SplitString(lines[i], '|', &fields);
      if (fields.empty())
        continue;  // Blank line.
      CHECK_EQ(12u, fields.size());

      FormMap data;
      data["NAME_FIRST"] = fields[0];
      data["NAME_MIDDLE"] = fields[1];
      data["NAME_LAST"] = fields[2];
      data["EMAIL_ADDRESS"] = fields[3];
      data["COMPANY_NAME"] = fields[4];
      data["ADDRESS_HOME_LINE1"] = fields[5];
      data["ADDRESS_HOME_LINE2"] = fields[6];
      data["ADDRESS_HOME_CITY"] = fields[7];
      data["ADDRESS_HOME_STATE"] = fields[8];
      data["ADDRESS_HOME_ZIP"] = fields[9];
      data["ADDRESS_HOME_COUNTRY"] = fields[10];
      data["PHONE_HOME_WHOLE_NUMBER"] = fields[11];

      FillFormAndSubmit("duplicate_profiles_test.html", data);
    }
    return lines.size();
  }

  void ExpectFieldValue(const std::wstring& field_name,
                        const std::string& expected_value) {
    std::string value;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractString(
        chrome::GetActiveWebContents(browser())->GetRenderViewHost(), L"",
        L"window.domAutomationController.send("
        L"document.getElementById('" + field_name + L"').value);", &value));
    EXPECT_EQ(expected_value, value);
  }

  RenderViewHost* render_view_host() {
    return chrome::GetActiveWebContents(browser())->GetRenderViewHost();
  }

  void SimulateURLFetch(bool success) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
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
    content::SimulateMouseClick(chrome::GetActiveWebContents(browser()));

    LOG(WARNING) << "Focusing the first name field.";
    bool result = false;
    ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
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
    content::WindowedNotificationObserver observer(
        notification_type, content::Source<RenderViewHost>(render_view_host()));
    content::SimulateKeyPress(chrome::GetActiveWebContents(
        browser()), key, false, false, false, false);
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
  net::TestURLFetcherFactory url_fetcher_factory_;
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
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
      render_view_host(), L"",
      L"domAutomationController.send(focused_fired);", &focused_fired));
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
      render_view_host(), L"",
      L"domAutomationController.send(unfocused_fired);", &unfocused_fired));
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
      render_view_host(), L"",
      L"domAutomationController.send(changed_select_fired);",
      &changed_select_fired));
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
      render_view_host(), L"",
      L"domAutomationController.send(unchanged_select_fired);",
      &unchanged_select_fired));
  EXPECT_FALSE(focused_fired);
  EXPECT_TRUE(unfocused_fired);
  EXPECT_TRUE(changed_select_fired);
  EXPECT_FALSE(unchanged_select_fired);

  // Unfocus the first name field. Its change event should fire.
  ASSERT_TRUE(content::ExecuteJavaScriptAndExtractBool(
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
  ASSERT_TRUE(content::ExecuteJavaScript(render_view_host(), L"",
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
  WebContents* tab = chrome::GetActiveWebContents(browser());
  tab->GetController().Reload(false);
  content::WaitForLoadStop(tab);

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
      InfoBarService::ForTab(chrome::GetActiveTabContents(browser()))->
          GetInfoBarDelegateAt(0)->AsTranslateInfoBarDelegate();

  ASSERT_TRUE(infobar != NULL);
  EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, infobar->type());

  // Simulate translation button press.
  infobar->Translate();

  // Simulate the translate script being retrieved.
  // Pass fake google.translate lib as the translate script.
  SimulateURLFetch(true);

  content::WindowedNotificationObserver translation_observer(
      chrome::NOTIFICATION_PAGE_TRANSLATED,
      content::NotificationService::AllSources());

  // Simulate translation to kick onTranslateElementLoad.
  // But right now, the call stucks here.
  // Once click the text field, it starts again.
  ASSERT_TRUE(content::ExecuteJavaScript(
      render_view_host(), L"",
      L"cr.googleTranslate.onTranslateElementLoad();"));

  // Simulate the render notifying the translation has been done.
  translation_observer.Wait();

  TryBasicFormFill();
}

// Test filling profiles with unicode strings and crazy characters.
// TODO(isherman): rewrite as unit test under PersonalDataManagerTest.
IN_PROC_BROWSER_TEST_F(AutofillTest, FillProfileCrazyCharacters) {
  std::vector<AutofillProfile> profiles;
  AutofillProfile profile1;
  profile1.SetInfo(NAME_FIRST, WideToUTF16(L"\u0623\u0648\u0628\u0627\u0645\u0627 \u064a\u0639\u062a\u0630\u0631 \u0647\u0627\u062a\u0641\u064a\u0627 \u0644\u0645\u0648\u0638\u0641\u0629 \u0633\u0648\u062f\u0627\u0621 \u0627\u0633\u062a\u0642\u0627\u0644\u062a \u0628\u0633\u0628\u0628 \u062a\u0635\u0631\u064a\u062d\u0627\u062a \u0645\u062c\u062a\u0632\u0623\u0629"));
  profile1.SetInfo(NAME_MIDDLE, WideToUTF16(L"BANK\xcBERF\xc4LLE"));
  profile1.SetInfo(EMAIL_ADDRESS, WideToUTF16(L"\uacbd\uc81c \ub274\uc2a4 \ub354\ubcf4\uae30@google.com"));
  profile1.SetInfo(ADDRESS_HOME_LINE1, WideToUTF16(L"\uad6d\uc815\uc6d0\xb7\uac80\ucc30, \ub178\ubb34\ud604\uc815\ubd80 \ub300\ubd81\uc811\ucd09 \ub2f4\ub2f9 \uc778\uc0ac\ub4e4 \uc870\uc0ac"));
  profile1.SetInfo(ADDRESS_HOME_CITY, WideToUTF16(L"\u653f\u5e9c\u4e0d\u6392\u9664\u7acb\u6cd5\u898f\u7ba1\u5c0e\u904a"));
  profile1.SetInfo(ADDRESS_HOME_ZIP, WideToUTF16(L"YOHO_54676"));
  profile1.SetInfo(PHONE_HOME_WHOLE_NUMBER, WideToUTF16(L"861088828000"));
  profile1.SetInfo(ADDRESS_HOME_COUNTRY, WideToUTF16(L"India"));
  profiles.push_back(profile1);

  AutofillProfile profile2;
  profile2.SetInfo(NAME_FIRST, WideToUTF16(L"\u4e0a\u6d77\u5e02\u91d1\u5c71\u533a \u677e\u9690\u9547\u4ead\u67ab\u516c\u8def1915\u53f7"));
  profile2.SetInfo(NAME_LAST, WideToUTF16(L"aguantó"));
  profile2.SetInfo(ADDRESS_HOME_ZIP, WideToUTF16(L"HOME 94043"));
  profiles.push_back(profile2);

  AutofillProfile profile3;
  profile3.SetInfo(EMAIL_ADDRESS, WideToUTF16(L"sue@example.com"));
  profile3.SetInfo(COMPANY_NAME, WideToUTF16(L"Company X"));
  profiles.push_back(profile3);

  AutofillProfile profile4;
  profile4.SetInfo(NAME_FIRST, WideToUTF16(L"Joe 3254"));
  profile4.SetInfo(NAME_LAST, WideToUTF16(L"\u8bb0\u8d262\u5e74\u591a"));
  profile4.SetInfo(ADDRESS_HOME_ZIP, WideToUTF16(L"\uff08\u90ae\u7f16\uff1a201504\uff09"));
  profile4.SetInfo(EMAIL_ADDRESS, WideToUTF16(L"télévision@example.com"));
  profile4.SetInfo(COMPANY_NAME, WideToUTF16(L"\u0907\u0932\u0947\u0915\u093f\u091f\u094d\u0930\u0928\u093f\u0915\u094d\u0938, \u0905\u092a\u094b\u0932\u094b \u091f\u093e\u092f\u0930\u094d\u0938 \u0906\u0926\u093f"));
  profiles.push_back(profile4);

  AutofillProfile profile5;
  profile5.SetInfo(NAME_FIRST, WideToUTF16(L"Larry"));
  profile5.SetInfo(NAME_LAST, WideToUTF16(L"\u0938\u094d\u091f\u093e\u0902\u092a \u0921\u094d\u092f\u0942\u091f\u0940"));
  profile5.SetInfo(ADDRESS_HOME_ZIP, WideToUTF16(L"111111111111110000GOOGLE"));
  profile5.SetInfo(EMAIL_ADDRESS, WideToUTF16(L"page@000000.com"));
  profile5.SetInfo(COMPANY_NAME, WideToUTF16(L"Google"));
  profiles.push_back(profile5);

  AutofillProfile profile6;
  profile6.SetInfo(NAME_FIRST, WideToUTF16(L"\u4e0a\u6d77\u5e02\u91d1\u5c71\u533a \u677e\u9690\u9547\u4ead\u67ab\u516c\u8def1915\u53f7"));
  profile6.SetInfo(NAME_LAST, WideToUTF16(L"\u0646\u062c\u0627\u0645\u064a\u0646\u0627 \u062f\u0639\u0645\u0647\u0627 \u0644\u0644\u0631\u0626\u064a\u0633 \u0627\u0644\u0633\u0648\u062f\u0627\u0646\u064a \u0639\u0645\u0631 \u0627\u0644\u0628\u0634\u064a\u0631"));
  profile6.SetInfo(ADDRESS_HOME_ZIP, WideToUTF16(L"HOME 94043"));
  profiles.push_back(profile6);

  AutofillProfile profile7;
  profile7.SetInfo(NAME_FIRST, WideToUTF16(L"&$%$$$ TESTO *&*&^&^& MOKO"));
  profile7.SetInfo(NAME_MIDDLE, WideToUTF16(L"WOHOOOO$$$$$$$$****"));
  profile7.SetInfo(EMAIL_ADDRESS, WideToUTF16(L"yuvu@example.com"));
  profile7.SetInfo(ADDRESS_HOME_LINE1, WideToUTF16(L"34544, anderson ST.(120230)"));
  profile7.SetInfo(ADDRESS_HOME_CITY, WideToUTF16(L"Sunnyvale"));
  profile7.SetInfo(ADDRESS_HOME_STATE, WideToUTF16(L"CA"));
  profile7.SetInfo(ADDRESS_HOME_ZIP, WideToUTF16(L"94086"));
  profile7.SetInfo(PHONE_HOME_WHOLE_NUMBER, WideToUTF16(L"15466784565"));
  profile7.SetInfo(ADDRESS_HOME_COUNTRY, WideToUTF16(L"United States"));
  profiles.push_back(profile7);

  SetProfiles(&profiles);
  ASSERT_EQ(profiles.size(), personal_data_manager()->profiles().size());
  for (size_t i = 0; i < profiles.size(); ++i)
    ASSERT_EQ(profiles[i], *personal_data_manager()->profiles()[i]);

  std::vector<CreditCard> cards;
  CreditCard card1;
  card1.SetInfo(CREDIT_CARD_NAME, WideToUTF16(L"\u751f\u6d3b\u5f88\u6709\u89c4\u5f8b \u4ee5\u73a9\u4e3a\u4e3b"));
  card1.SetInfo(CREDIT_CARD_NUMBER, WideToUTF16(L"6011111111111117"));
  card1.SetInfo(CREDIT_CARD_EXP_MONTH, WideToUTF16(L"12"));
  card1.SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, WideToUTF16(L"2011"));
  cards.push_back(card1);

  CreditCard card2;
  card2.SetInfo(CREDIT_CARD_NAME, WideToUTF16(L"John Williams"));
  card2.SetInfo(CREDIT_CARD_NUMBER, WideToUTF16(L"WokoAwesome12345"));
  card2.SetInfo(CREDIT_CARD_EXP_MONTH, WideToUTF16(L"10"));
  card2.SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, WideToUTF16(L"2015"));
  cards.push_back(card2);

  CreditCard card3;
  card3.SetInfo(CREDIT_CARD_NAME, WideToUTF16(L"\u0623\u062d\u0645\u062f\u064a \u0646\u062c\u0627\u062f \u0644\u0645\u062d\u0627\u0648\u0644\u0647 \u0627\u063a\u062a\u064a\u0627\u0644 \u0641\u064a \u0645\u062f\u064a\u0646\u0629 \u0647\u0645\u062f\u0627\u0646 "));
  card3.SetInfo(CREDIT_CARD_NUMBER, WideToUTF16(L"\u092a\u0941\u0928\u0930\u094d\u091c\u0940\u0935\u093f\u0924 \u0939\u094b\u0917\u093e \u0928\u093e\u0932\u0902\u0926\u093e"));
  card3.SetInfo(CREDIT_CARD_EXP_MONTH, WideToUTF16(L"10"));
  card3.SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, WideToUTF16(L"2015"));
  cards.push_back(card3);

  CreditCard card4;
  card4.SetInfo(CREDIT_CARD_NAME, WideToUTF16(L"\u039d\u03ad\u03b5\u03c2 \u03c3\u03c5\u03b3\u03c7\u03c9\u03bd\u03b5\u03cd\u03c3\u03b5\u03b9\u03c2 \u03ba\u03b1\u03b9 \u03ba\u03b1\u03c4\u03b1\u03c1\u03b3\u03ae\u03c3\u03b5\u03b9\u03c2"));
  card4.SetInfo(CREDIT_CARD_NUMBER, WideToUTF16(L"00000000000000000000000"));
  card4.SetInfo(CREDIT_CARD_EXP_MONTH, WideToUTF16(L"01"));
  card4.SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, WideToUTF16(L"2016"));
  cards.push_back(card4);

  SetCards(&cards);
  ASSERT_EQ(cards.size(), personal_data_manager()->credit_cards().size());
  for (size_t i = 0; i < cards.size(); ++i)
    ASSERT_EQ(cards[i], *personal_data_manager()->credit_cards()[i]);
}

// Test filling in invalid values for profiles are saved as-is. Phone
// information entered into the prefs UI is not validated or rejected except for
// duplicates.
// TODO(isherman): rewrite as WebUI test?
IN_PROC_BROWSER_TEST_F(AutofillTest, Invalid) {
  // First try profiles with invalid ZIP input.
  AutofillProfile without_invalid;
  without_invalid.SetInfo(NAME_FIRST, ASCIIToUTF16("Will"));
  without_invalid.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Sunnyvale"));
  without_invalid.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  without_invalid.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("my_zip"));
  without_invalid.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("United States"));

  AutofillProfile with_invalid = without_invalid;
  with_invalid.SetInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("Invalid_Phone_Number"));
  SetProfile(with_invalid);

  ASSERT_EQ(1u, personal_data_manager()->profiles().size());
  AutofillProfile profile = *personal_data_manager()->profiles()[0];
  ASSERT_NE(without_invalid.GetInfo(PHONE_HOME_WHOLE_NUMBER),
            profile.GetInfo(PHONE_HOME_WHOLE_NUMBER));
}

// Test invalid credit card numbers typed in prefs should be saved as-is.
// TODO(isherman): rewrite as WebUI test?
IN_PROC_BROWSER_TEST_F(AutofillTest, PrefsStringSavedAsIs) {
  CreditCard card;
  card.SetInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("Not_0123-5Checked"));
  SetCard(card);

  ASSERT_EQ(1u, personal_data_manager()->credit_cards().size());
  ASSERT_EQ(card, *personal_data_manager()->credit_cards()[0]);
}

// Test credit card info with an invalid number is not aggregated.
// When filling out a form with an invalid credit card number (one that does not
// pass the Luhn test) the credit card info should not be saved into Autofill
// preferences.
IN_PROC_BROWSER_TEST_F(AutofillTest, InvalidCreditCardNumberIsNotAggregated) {
  ASSERT_TRUE(test_server()->Start());
  std::string card("4408 0412 3456 7890");
  ASSERT_FALSE(CreditCard::IsValidCreditCardNumber(ASCIIToUTF16(card)));
  SubmitCreditCard("Bob Smith", card.c_str(), "12", "2014");
  ASSERT_EQ(0u,
            InfoBarService::ForTab(chrome::GetActiveTabContents(browser()))->
                GetInfoBarCount());
}

// Test whitespaces and separator chars are stripped for valid CC numbers.
// The credit card numbers used in this test pass the Luhn test. For reference:
// http://www.merriampark.com/anatomycc.htm
IN_PROC_BROWSER_TEST_F(AutofillTest,
                       WhitespacesAndSeparatorCharsStrippedForValidCCNums) {
  ASSERT_TRUE(test_server()->Start());
  SubmitCreditCard("Bob Smith", "4408 0412 3456 7893", "12", "2014");
  SubmitCreditCard("Jane Doe", "4417-1234-5678-9113", "10", "2013");

  ASSERT_EQ(2u, personal_data_manager()->credit_cards().size());
  string16 cc1 = personal_data_manager()->credit_cards()[0]->GetInfo(
      CREDIT_CARD_NUMBER);
  ASSERT_TRUE(CreditCard::IsValidCreditCardNumber(cc1));
  string16 cc2 = personal_data_manager()->credit_cards()[1]->GetInfo(
      CREDIT_CARD_NUMBER);
  ASSERT_TRUE(CreditCard::IsValidCreditCardNumber(cc2));
}

// Test that Autofill aggregates a minimum valid profile.
// The minimum required address fields must be specified: First Name, Last Name,
// Address Line 1, City, Zip Code, and State.
IN_PROC_BROWSER_TEST_F(AutofillTest, AggregatesMinValidProfile) {
  ASSERT_TRUE(test_server()->Start());
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "94043";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_EQ(1u, personal_data_manager()->profiles().size());
}

// Test Autofill does not aggregate profiles with no address info.
// The minimum required address fields must be specified: First Name, Last Name,
// Address Line 1, City, Zip Code, and State.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfilesNotAggregatedWithNoAddress) {
  ASSERT_TRUE(test_server()->Start());
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["EMAIL_ADDRESS"] = "bsmith@example.com";
  data["COMPANY_NAME"] = "Mountain View";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["PHONE_HOME_WHOLE_NUMBER"] = "650-555-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_TRUE(personal_data_manager()->profiles().empty());
}

// Test Autofill does not aggregate profiles with an invalid email.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfilesNotAggregatedWithInvalidEmail) {
  ASSERT_TRUE(test_server()->Start());
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["EMAIL_ADDRESS"] = "garbage";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "San Jose";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "95110";
  data["COMPANY_NAME"] = "Company X";
  data["PHONE_HOME_WHOLE_NUMBER"] = "408-871-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_TRUE(personal_data_manager()->profiles().empty());
}

// Test phone fields parse correctly from a given profile.
// The high level key presses execute the following: Select the first text
// field, invoke the autofill popup list, select the first profile within the
// list, and commit to the profile to populate the form.
IN_PROC_BROWSER_TEST_F(AutofillTest, ComparePhoneNumbers) {
  ASSERT_TRUE(test_server()->Start());

  AutofillProfile profile;
  profile.SetInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
  profile.SetInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 H St."));
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("San Jose"));
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("95110"));
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1-408-555-4567"));
  SetProfile(profile);

  GURL url = test_server()->GetURL("files/autofill/form_phones.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ExpectFieldValue(L"NAME_FIRST", "Bob");
  ExpectFieldValue(L"NAME_LAST", "Smith");
  ExpectFieldValue(L"ADDRESS_HOME_LINE1", "1234 H St.");
  ExpectFieldValue(L"ADDRESS_HOME_CITY", "San Jose");
  ExpectFieldValue(L"ADDRESS_HOME_STATE", "CA");
  ExpectFieldValue(L"ADDRESS_HOME_ZIP", "95110");
  ExpectFieldValue(L"PHONE_HOME_WHOLE_NUMBER", "14085554567");
  ExpectFieldValue(L"PHONE_HOME_CITY_CODE-1", "408");
  ExpectFieldValue(L"PHONE_HOME_CITY_CODE-2", "408");
  ExpectFieldValue(L"PHONE_HOME_NUMBER", "5554567");
  ExpectFieldValue(L"PHONE_HOME_NUMBER_3-1", "555");
  ExpectFieldValue(L"PHONE_HOME_NUMBER_3-2", "555");
  ExpectFieldValue(L"PHONE_HOME_NUMBER_4-1", "4567");
  ExpectFieldValue(L"PHONE_HOME_NUMBER_4-2", "4567");
  ExpectFieldValue(L"PHONE_HOME_EXT-1", "");
  ExpectFieldValue(L"PHONE_HOME_EXT-2", "");
  ExpectFieldValue(L"PHONE_HOME_COUNTRY_CODE-1", "1");
}

// Test profile is saved if phone number is valid in selected country.
// The data file contains two profiles with valid phone numbers and two
// profiles with invalid phone numbers from their respective country.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfileSavedWithValidCountryPhone) {
  ASSERT_TRUE(test_server()->Start());
  std::vector<FormMap> profiles;

  FormMap data1;
  data1["NAME_FIRST"] = "Bob";
  data1["NAME_LAST"] = "Smith";
  data1["ADDRESS_HOME_LINE1"] = "123 Cherry Ave";
  data1["ADDRESS_HOME_CITY"] = "Mountain View";
  data1["ADDRESS_HOME_STATE"] = "CA";
  data1["ADDRESS_HOME_ZIP"] = "94043";
  data1["ADDRESS_HOME_COUNTRY"] = "United States";
  data1["PHONE_HOME_WHOLE_NUMBER"] = "408-871-4567";
  profiles.push_back(data1);

  FormMap data2;
  data2["NAME_FIRST"] = "John";
  data2["NAME_LAST"] = "Doe";
  data2["ADDRESS_HOME_LINE1"] = "987 H St";
  data2["ADDRESS_HOME_CITY"] = "San Jose";
  data2["ADDRESS_HOME_STATE"] = "CA";
  data2["ADDRESS_HOME_ZIP"] = "95510";
  data2["ADDRESS_HOME_COUNTRY"] = "United States";
  data2["PHONE_HOME_WHOLE_NUMBER"] = "408-123-456";
  profiles.push_back(data2);

  FormMap data3;
  data3["NAME_FIRST"] = "Jane";
  data3["NAME_LAST"] = "Doe";
  data3["ADDRESS_HOME_LINE1"] = "1523 Garcia St";
  data3["ADDRESS_HOME_CITY"] = "Mountain View";
  data3["ADDRESS_HOME_STATE"] = "CA";
  data3["ADDRESS_HOME_ZIP"] = "94043";
  data3["ADDRESS_HOME_COUNTRY"] = "Germany";
  data3["PHONE_HOME_WHOLE_NUMBER"] = "+49 40-80-81-79-000";
  profiles.push_back(data3);

  FormMap data4;
  data4["NAME_FIRST"] = "Bonnie";
  data4["NAME_LAST"] = "Smith";
  data4["ADDRESS_HOME_LINE1"] = "6723 Roadway Rd";
  data4["ADDRESS_HOME_CITY"] = "San Jose";
  data4["ADDRESS_HOME_STATE"] = "CA";
  data4["ADDRESS_HOME_ZIP"] = "95510";
  data4["ADDRESS_HOME_COUNTRY"] = "Germany";
  data4["PHONE_HOME_WHOLE_NUMBER"] = "+21 08450 777 777";
  profiles.push_back(data4);

  for (size_t i = 0; i < profiles.size(); ++i)
    FillFormAndSubmit("autofill_test_form.html", profiles[i]);

  ASSERT_EQ(2u, personal_data_manager()->profiles().size());
  ASSERT_EQ(ASCIIToUTF16("(408) 871-4567"),
            personal_data_manager()->profiles()[0]->GetInfo(
                PHONE_HOME_WHOLE_NUMBER));
  ASSERT_EQ(ASCIIToUTF16("+49 40/808179000"),
            personal_data_manager()->profiles()[1]->GetInfo(
                PHONE_HOME_WHOLE_NUMBER));
}

// Test Autofill appends country codes to aggregated phone numbers.
// The country code is added for the following case:
//   The phone number contains the correct national number size and
//   is a valid format.
IN_PROC_BROWSER_TEST_F(AutofillTest, AppendCountryCodeForAggregatedPhones) {
  ASSERT_TRUE(test_server()->Start());
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "San Jose";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "95110";
  data["ADDRESS_HOME_COUNTRY"] = "Germany";
  data["PHONE_HOME_WHOLE_NUMBER"] = "(08) 450 777-777";
  FillFormAndSubmit("autofill_test_form.html", data);

  ASSERT_EQ(1u, personal_data_manager()->profiles().size());
  string16 phone =
      personal_data_manager()->profiles()[0]->GetInfo(PHONE_HOME_WHOLE_NUMBER);
  ASSERT_TRUE(StartsWith(phone, ASCIIToUTF16("+49"), true));
}

// Test CC info not offered to be saved when autocomplete=off for CC field.
// If the credit card number field has autocomplete turned off, then the credit
// card infobar should not offer to save the credit card info. The credit card
// number must be a valid Luhn number.
IN_PROC_BROWSER_TEST_F(AutofillTest, CCInfoNotStoredWhenAutocompleteOff) {
  ASSERT_TRUE(test_server()->Start());
  FormMap data;
  data["CREDIT_CARD_NAME"] = "Bob Smith";
  data["CREDIT_CARD_NUMBER"] = "4408041234567893";
  data["CREDIT_CARD_EXP_MONTH"] = "12";
  data["CREDIT_CARD_EXP_4_DIGIT_YEAR"] = "2014";
  FillFormAndSubmit("cc_autocomplete_off_test.html", data);

  ASSERT_EQ(0u,
            InfoBarService::ForTab(chrome::GetActiveTabContents(browser()))->
                GetInfoBarCount());
}

// Test that Autofill does not fill in read-only fields.
IN_PROC_BROWSER_TEST_F(AutofillTest, NoAutofillForReadOnlyFields) {
  ASSERT_TRUE(test_server()->Start());

  std::string addr_line1("1234 H St.");

  AutofillProfile profile;
  profile.SetInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
  profile.SetInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetInfo(EMAIL_ADDRESS, ASCIIToUTF16("bsmith@gmail.com"));
  profile.SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16(addr_line1));
  profile.SetInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("San Jose"));
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  profile.SetInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("95110"));
  profile.SetInfo(COMPANY_NAME, ASCIIToUTF16("Company X"));
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("408-871-4567"));
  SetProfile(profile);

  GURL url = test_server()->GetURL("files/autofill/read_only_field_test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("firstname");

  ExpectFieldValue(L"email", "");
  ExpectFieldValue(L"address", addr_line1);
}

// Test form is fillable from a profile after form was reset.
// Steps:
//   1. Fill form using a saved profile.
//   2. Reset the form.
//   3. Fill form using a saved profile.
IN_PROC_BROWSER_TEST_F(AutofillTest, FormFillableOnReset) {
  ASSERT_TRUE(test_server()->Start());

  CreateTestProfile();

  GURL url = test_server()->GetURL("files/autofill/autofill_test_form.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ASSERT_TRUE(content::ExecuteJavaScript(
      chrome::GetActiveWebContents(browser())->GetRenderViewHost(), L"",
      L"document.getElementById('testform').reset()"));

  PopulateForm("NAME_FIRST");

  ExpectFieldValue(L"NAME_FIRST", "Milton");
  ExpectFieldValue(L"NAME_LAST", "Waddams");
  ExpectFieldValue(L"EMAIL_ADDRESS", "red.swingline@initech.com");
  ExpectFieldValue(L"ADDRESS_HOME_LINE1", "4120 Freidrich Lane");
  ExpectFieldValue(L"ADDRESS_HOME_CITY", "Austin");
  ExpectFieldValue(L"ADDRESS_HOME_STATE", "Texas");
  ExpectFieldValue(L"ADDRESS_HOME_ZIP", "78744");
  ExpectFieldValue(L"ADDRESS_HOME_COUNTRY", "United States");
  ExpectFieldValue(L"PHONE_HOME_WHOLE_NUMBER", "5125551234");
}

// Test Autofill distinguishes a middle initial in a name.
IN_PROC_BROWSER_TEST_F(AutofillTest, DistinguishMiddleInitialWithinName) {
  ASSERT_TRUE(test_server()->Start());

  CreateTestProfile();

  GURL url = test_server()->GetURL(
      "files/autofill/autofill_middleinit_form.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ExpectFieldValue(L"NAME_MIDDLE", "C");
}

// Test forms with multiple email addresses are filled properly.
// Entire form should be filled with one user gesture.
IN_PROC_BROWSER_TEST_F(AutofillTest, MultipleEmailFilledByOneUserGesture) {
  ASSERT_TRUE(test_server()->Start());

  std::string email("bsmith@gmail.com");

  AutofillProfile profile;
  profile.SetInfo(NAME_FIRST, ASCIIToUTF16("Bob"));
  profile.SetInfo(NAME_LAST, ASCIIToUTF16("Smith"));
  profile.SetInfo(EMAIL_ADDRESS, ASCIIToUTF16(email));
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("4088714567"));
  SetProfile(profile);

  GURL url = test_server()->GetURL(
      "files/autofill/autofill_confirmemail_form.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  ExpectFieldValue(L"EMAIL_CONFIRM", email);
  // TODO(isherman): verify entire form.
}

// Test profile not aggregated if email found in non-email field.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfileWithEmailInOtherFieldNotSaved) {
  ASSERT_TRUE(test_server()->Start());

  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "bsmith@gmail.com";
  data["ADDRESS_HOME_CITY"] = "San Jose";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "95110";
  data["COMPANY_NAME"] = "Company X";
  data["PHONE_HOME_WHOLE_NUMBER"] = "408-871-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_EQ(0u, personal_data_manager()->profiles().size());
}

// Test latency time on form submit with lots of stored Autofill profiles.
// This test verifies when a profile is selected from the Autofill dictionary
// that consists of thousands of profiles, the form does not hang after being
// submitted.
IN_PROC_BROWSER_TEST_F(AutofillTest, FormFillLatencyAfterSubmit) {
  ASSERT_TRUE(test_server()->Start());

  std::vector<std::string> cities;
  cities.push_back("San Jose");
  cities.push_back("San Francisco");
  cities.push_back("Sacramento");
  cities.push_back("Los Angeles");

  std::vector<std::string> streets;
  streets.push_back("St");
  streets.push_back("Ave");
  streets.push_back("Ln");
  streets.push_back("Ct");

  const int kNumProfiles = 1500;
  base::Time start_time = base::Time::Now();
  std::vector<AutofillProfile> profiles;
  for (int i = 0; i < kNumProfiles; i++) {
    AutofillProfile profile;
    string16 name(base::IntToString16(i));
    string16 email(name + ASCIIToUTF16("@example.com"));
    string16 street = ASCIIToUTF16(
        base::IntToString(base::RandInt(0, 10000)) + " " +
        streets[base::RandInt(0, streets.size() - 1)]);
    string16 city = ASCIIToUTF16(cities[base::RandInt(0, cities.size() - 1)]);
    string16 zip(base::IntToString16(base::RandInt(0, 10000)));
    profile.SetInfo(NAME_FIRST, name);
    profile.SetInfo(EMAIL_ADDRESS, email);
    profile.SetInfo(ADDRESS_HOME_LINE1, street);
    profile.SetInfo(ADDRESS_HOME_CITY, city);
    profile.SetInfo(ADDRESS_HOME_STATE, WideToUTF16(L"CA"));
    profile.SetInfo(ADDRESS_HOME_ZIP, zip);
    profile.SetInfo(ADDRESS_HOME_COUNTRY, WideToUTF16(L"United States"));
    profiles.push_back(profile);
  }
  SetProfiles(&profiles);
  // TODO(isherman): once we're sure this test doesn't timeout on any bots, this
  // can be removd.
  LOG(INFO) << "Created " << kNumProfiles << " profiles in " <<
               (base::Time::Now() - start_time).InSeconds() << " seconds.";

  GURL url = test_server()->GetURL(
      "files/autofill/latency_after_submit_test.html");
  ui_test_utils::NavigateToURL(browser(), url);
  PopulateForm("NAME_FIRST");

  content::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &chrome::GetActiveWebContents(browser())->GetController()));

  ASSERT_TRUE(content::ExecuteJavaScript(
      render_view_host(), L"",
      ASCIIToWide("document.getElementById('testform').submit();")));
  // This will ensure the test didn't hang.
  load_stop_observer.Wait();
}

// Test that profiles merge for aggregated data with same address.
// The criterion for when two profiles are expected to be merged is when their
// 'Address Line 1' and 'City' data match. When two profiles are merged, any
// remaining address fields are expected to be overwritten. Any non-address
// fields should accumulate multi-valued data.
IN_PROC_BROWSER_TEST_F(AutofillTest, MergeAggregatedProfilesWithSameAddress) {
  AggregateProfilesIntoAutofillPrefs("dataset_2.txt");

  ASSERT_EQ(3u, personal_data_manager()->profiles().size());
}

// Test profiles are not merged without mininum address values.
// Mininum address values needed during aggregation are: address line 1, city,
// state, and zip code.
// Profiles are merged when data for address line 1 and city match.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfilesNotMergedWhenNoMinAddressData) {
  AggregateProfilesIntoAutofillPrefs("dataset_no_address.txt");

  ASSERT_EQ(0u, personal_data_manager()->profiles().size());
}

#if defined(OS_LINUX)
// Has been observed to fail on linux.  crbug.com/146688
#define MAYBE_MergeAggregatedDuplicatedProfiles \
    DISABLED_MergeAggregatedDuplicatedProfiles
#else
#define MAYBE_MergeAggregatedDuplicatedProfiles \
    MergeAggregatedDuplicatedProfiles
#endif

// Test Autofill ability to merge duplicate profiles and throw away junk.
// TODO(isherman): this looks redundant, consider removing.
IN_PROC_BROWSER_TEST_F(AutofillTest, MAYBE_MergeAggregatedDuplicatedProfiles) {
  int num_of_profiles =
      AggregateProfilesIntoAutofillPrefs("dataset_no_address.txt");

  ASSERT_GT(num_of_profiles,
            static_cast<int>(personal_data_manager()->profiles().size()));
}
