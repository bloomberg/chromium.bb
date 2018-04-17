// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_manager_test_delegate.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/state_names.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

#define JAVASCRIPT_SNIPPET_CLICK_ON_ELEMENT "target.click();"
#define JAVASCRIPT_SNIPPET_SELECT_DROP_DOWN_OPTION(selectedIndex)             \
  base::StringPrintf(                                                         \
      "automation_helper.selectOptionFromDropDownElementByIndex(target, %d)", \
      selectedIndex)
#define JAVASCRIPT_SNIPPET_GET_ELEMENT_VALUE "return target.value;"
#define JAVASCRIPT_SNIPPET_GET_ELEMENT_AUTOFILL_PREDICTION \
  "return target.getAttribute('autofill-prediction');"

namespace {
constexpr int kDefaultActionTimeoutSeconds = 30;

struct AutofillTarget {
  std::string css_selector;
  std::string expected_autofill_field_type;
  std::string expected_value;
  bool ignore_case = false;
  std::vector<std::string> element_ready_function_override;
};
}  // namespace

namespace autofill {
class AutofillCapturedSitesInteractiveTest
    : public AutofillUiTest,
      public content::WebContentsObserver {
 protected:
  AutofillCapturedSitesInteractiveTest()
      : profile_(test::GetFullProfile()),
        card_(test::GetRandomCreditCard(CreditCard::LOCAL_CARD)),
        phone_(PhoneNumber(&profile_)) {}
  ~AutofillCapturedSitesInteractiveTest() override{};

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    AutofillUiTest::SetUpOnMainThread();
    SetupTestProfile();
    ASSERT_TRUE(InstallWebPageReplayServerRootCert())
        << "Cannot install the root certificate "
        << "for the local web page replay server.";
    CleanupSiteData();
  }

  void TearDownOnMainThread() override {
    // If there are still cookies at the time the browser test shuts down,
    // Chrome's SQL lite persistent cookie store will crash.
    CleanupSiteData();
    EXPECT_TRUE(StopWebPageReplayServer())
        << "Cannot stop the local Web Page Replay server.";
    EXPECT_TRUE(RemoveWebPageReplayServerRootCert())
        << "Cannot remove the root certificate "
        << "for the local Web Page Replay server.";

    AutofillUiTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the autofill show typed prediction feature. When active this
    // feature forces input elements on a form to display their autofill type
    // prediction. Test will check this attribute on all the relevant input
    // elements in a form to determine if the form is ready for interaction.
    feature_list_.InitAndEnableFeature(features::kAutofillShowTypePredictions);
    command_line->AppendSwitch(switches::kShowAutofillTypePredictions);

    // Direct traffic to the Web Page Replay server.
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        base::StringPrintf(
            "MAP *:80 127.0.0.1:%d,"
            "MAP *:443 127.0.0.1:%d,"
            // uncomment to use the live autofill prediction server.
            //"EXCLUDE clients1.google.com,"
            "EXCLUDE localhost",
            host_http_port_, host_https_port_));
  }

  bool StartWebPageReplayServer(std::string replay_file) {
    std::vector<std::string> args;
    base::FilePath src_dir;
    CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    args.push_back(base::StringPrintf("--http_port=%d", host_http_port_));
    args.push_back(base::StringPrintf("--https_port=%d", host_https_port_));
    args.push_back(base::StringPrintf(
        "--inject_scripts=%s,%s",
        src_dir.AppendASCII("third_party/catapult/web_page_replay_go")
            .AppendASCII("deterministic.js")
            .value()
            .c_str(),
        src_dir
            .AppendASCII("chrome/test/data/web_page_replay_go_helper_scripts")
            .AppendASCII("automation_helper.js")
            .value()
            .c_str()));

    // Specify the replay file.
    args.push_back(base::StringPrintf(
        "%s", src_dir.AppendASCII("chrome/test/data/autofill/captured_sites")
                  .AppendASCII(replay_file)
                  .value()
                  .c_str()));

    web_page_replay_server_ = RunWebPageReplayCmd("replay", args);

    // Sleep 20 seconds to wait for the web page replay server to start.
    // TODO(uwyiming): create a process std stream reader class to use the
    // process output to determine when the server is ready
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(20));

    return web_page_replay_server_.IsValid();
  }

  bool StopWebPageReplayServer() {
    if (web_page_replay_server_.IsValid())
      return web_page_replay_server_.Terminate(0, true);
    // The test server hasn't started, no op.
    return true;
  }

  bool WaitForStateChange(
      const std::vector<std::string> state_assertions,
      const base::TimeDelta timeout =
          base::TimeDelta::FromSeconds(kDefaultActionTimeoutSeconds)) {
    const base::TimeTicks start_time = base::TimeTicks::Now();
    while (!AllAssertionsPassed(state_assertions)) {
      if (base::TimeTicks::Now() - start_time > timeout) {
        ADD_FAILURE() << "State change hasn't completed within timeout.";
        return false;
      }
      base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
    }
    return true;
  }

  bool SelectDropDownOption(
      const std::string& dropDownElementSelector,
      const int selectedIndex,
      const base::TimeDelta timeToWaitForElement =
          base::TimeDelta::FromSeconds(kDefaultActionTimeoutSeconds)) {
    return ExecuteJavaScriptOnElementBySelector(
        dropDownElementSelector,
        JAVASCRIPT_SNIPPET_SELECT_DROP_DOWN_OPTION(selectedIndex),
        timeToWaitForElement);
  }

  bool ClickElementBySelector(
      const std::string& element_selector,
      const base::TimeDelta timeToWaitForElement =
          base::TimeDelta::FromSeconds(kDefaultActionTimeoutSeconds)) {
    return ExecuteJavaScriptOnElementBySelector(
        element_selector, JAVASCRIPT_SNIPPET_CLICK_ON_ELEMENT,
        timeToWaitForElement);
  }

  bool TestAutofill(const std::vector<AutofillTarget> autofill_targets,
                    std::string text_input_element_selector) {
    if (autofill_targets.size() == 0)
      return false;

    // First, ensure that all the autofill fields are ready for interaction
    // on the page.
    std::vector<std::string> state_assertions;
    for (const AutofillTarget autofill_field : autofill_targets) {
      if (autofill_field.element_ready_function_override.empty()) {
        state_assertions.push_back(base::StringPrintf(
            "return automation_helper.isElementWithSelectorReady(`%s`);",
            autofill_field.css_selector.c_str()));
        state_assertions.push_back(
            base::StringPrintf("var attr = document.querySelector(`%s`)"
                               ".getAttribute('autofill-prediction');"
                               "return (attr !== undefined && attr !== null);",
                               autofill_field.css_selector.c_str()));
      } else {
        for (const std::string ready_function :
             autofill_field.element_ready_function_override) {
          state_assertions.push_back(ready_function);
        }
      }
    }

    if (WaitForStateChange(state_assertions,
                           base::TimeDelta::FromSeconds(20))) {
      // Verify that each autofill field is assigned the correct prediction
      // attribute.
      for (const AutofillTarget autofill_target : autofill_targets) {
        if (!ExpectElementPropertyEquals(
                autofill_target.css_selector.c_str(),
                JAVASCRIPT_SNIPPET_GET_ELEMENT_AUTOFILL_PREDICTION,
                autofill_target.expected_autofill_field_type, true))
          return false;
      }

      // Invoke autofill.
      if (TryFillForm(text_input_element_selector, 5)) {
        // Now go through the targets one by one and ensure that each has
        // the expected value.
        for (const AutofillTarget autofill_target : autofill_targets) {
          if (!ExpectElementPropertyEquals(autofill_target.css_selector.c_str(),
                                           JAVASCRIPT_SNIPPET_GET_ELEMENT_VALUE,
                                           autofill_target.expected_value,
                                           autofill_target.ignore_case))
            return false;
        }
        return true;
      }
    }

    return false;
  }

  CreditCard* credit_card() { return &card_; }

  AutofillProfile* profile() { return &profile_; }

  PhoneNumber* phone() { return &phone_; }

 private:
  void SetupTestProfile() {
    test::SetProfileInfo(&profile_, "Milton", "C.", "Waddams",
                         "red.swingline@initech.com", "Initech",
                         "4120 Freidrich Lane", "Apt 8", "Austin", "Texas",
                         "78744", "US", "5125551234");
    AddTestAutofillData(browser(), profile_, card_);
    phone_.SetInfo(
        AutofillType(PHONE_HOME_WHOLE_NUMBER),
        profile_.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
        base::UTF16ToASCII(profile_.GetRawInfo(ADDRESS_HOME_COUNTRY)));
  }

  bool InstallWebPageReplayServerRootCert() {
    return RunWebPageReplayCmdAndWaitForExit("installroot",
                                             std::vector<std::string>());
  }

  bool RemoveWebPageReplayServerRootCert() {
    return RunWebPageReplayCmdAndWaitForExit("removeroot",
                                             std::vector<std::string>());
  }

  bool RunWebPageReplayCmdAndWaitForExit(
      std::string cmd,
      std::vector<std::string> args,
      const base::TimeDelta timeout = base::TimeDelta::FromSeconds(5)) {
    bool succeeded = false;
    base::Process process = RunWebPageReplayCmd(cmd, args);
    if (process.IsValid()) {
      int exit_code;
      if (process.WaitForExitWithTimeout(timeout, &exit_code))
        succeeded = (exit_code == 0);
    }
    return succeeded;
  }

  base::Process RunWebPageReplayCmd(std::string cmd,
                                    std::vector<std::string> args) {
    base::LaunchOptions options = base::LaunchOptionsForTest();
    base::FilePath exe_dir;
    CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &exe_dir));
    base::FilePath web_page_replay_binary_dir = exe_dir.AppendASCII(
        "third_party/catapult/telemetry/telemetry/internal/bin");
    options.current_directory = web_page_replay_binary_dir;

#if defined(OS_WIN)
    std::string wpr_executable_binary = "win/x86_64/wpr";
#elif defined(OS_MACOSX)
    std::string wpr_executable_binary = "mac/x86_64/wpr";
#elif defined(OS_POSIX)
    std::string wpr_executable_binary = "linux/x86_64/wpr";
#else
#error Plaform is not supported.
#endif
    base::CommandLine full_command(
        web_page_replay_binary_dir.AppendASCII(wpr_executable_binary));
    full_command.AppendArg(cmd);

    // Ask web page replay to use the custom certifcate and key files used to
    // make the web page captures.
    // The custom cert and key files are different from those of the offical
    // WPR releases. The custom files are made to work on iOS.
    base::FilePath src_dir;
    CHECK(PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    base::FilePath web_page_replay_support_file_dir = src_dir.AppendASCII(
        "components/test/data/autofill/web_page_replay_support_files");
    full_command.AppendArg(base::StringPrintf(
        "--https_cert_file=%s",
        web_page_replay_support_file_dir.AppendASCII("wpr_cert.pem")
            .value()
            .c_str()));
    full_command.AppendArg(base::StringPrintf(
        "--https_key_file=%s",
        web_page_replay_support_file_dir.AppendASCII("wpr_key.pem")
            .value()
            .c_str()));

    for (const auto arg : args)
      full_command.AppendArg(arg);

    return base::LaunchProcess(full_command, options);
  }

  bool AllAssertionsPassed(const std::vector<std::string> assertions)
      WARN_UNUSED_RESULT {
    for (const std::string assertion : assertions) {
      bool assertion_passed = false;
      EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
          GetWebContents(),
          base::StringPrintf("window.domAutomationController.send("
                             "    (function() {"
                             "      try {"
                             "        %s"
                             "      } catch (ex) {}"
                             "      return false;"
                             "    })());",
                             assertion.c_str()),
          &assertion_passed));
      if (!assertion_passed) {
        LOG(ERROR) << "'" << assertion << "' failed!";
        return false;
      }
    }
    return true;
  }

  void CleanupSiteData() {
    // Navigate to about:blank, then clear the browser cache.
    // Navigating to about:blank before clearing the cache ensures that
    // the cleanup is thorough and nothing is held.
    ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
    content::BrowsingDataRemover* remover =
        content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveAndReply(
        base::Time(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_COOKIES,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  bool ExecuteJavaScriptOnElementBySelector(
      const std::string& element_selector,
      const std::string& execute_function_body,
      const base::TimeDelta timeToWaitForElement =
          base::TimeDelta::FromSeconds(kDefaultActionTimeoutSeconds)) {
    // First, ensure that the element is present on the page.
    std::vector<std::string> state_assertions;
    state_assertions.push_back("return document.readyState === 'complete';");
    state_assertions.push_back(base::StringPrintf(
        "return automation_helper.isElementWithSelectorReady(`%s`);",
        element_selector.c_str()));
    if (WaitForStateChange(state_assertions, timeToWaitForElement)) {
      std::string js(base::StringPrintf(
          "try {"
          "  var element = document.querySelector(`%s`);"
          "  (function(target) { %s })(element);"
          "} catch(ex) {}",
          element_selector.c_str(), execute_function_body.c_str()));
      return content::ExecuteScript(GetWebContents(), js);
    }
    return false;
  }

  bool ExpectElementPropertyEquals(
      const std::string& element_selector,
      const std::string& get_property_function_body,
      const std::string& expected_value,
      bool ignoreCase = false) {
    std::string value;
    if (content::ExecuteScriptAndExtractString(
            GetWebContents(),
            base::StringPrintf("window.domAutomationController.send("
                               "    (function() {"
                               "      try {"
                               "        var element = function() {"
                               "          return document.querySelector(`%s`);"
                               "        }();"
                               "        return function(target){%s}(element);"
                               "      } catch (ex) {}"
                               "      return 'Exception encountered';"
                               "    })());",
                               element_selector.c_str(),
                               get_property_function_body.c_str()),
            &value)) {
      if (ignoreCase) {
        EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(expected_value, value))
            << "Field selector: `" << element_selector << "`, "
            << "Expected: " << expected_value << ", actual: " << value;
      } else {
        EXPECT_EQ(expected_value, value)
            << "Field selector: `" << element_selector << "`, ";
      }
      return true;
    }
    LOG(ERROR) << element_selector << ", " << get_property_function_body;
    return false;
  }

  // The Web Page Replay server that will be serving the captured sites
  base::Process web_page_replay_server_;
  const int host_http_port_ = 8080;
  const int host_https_port_ = 8081;

  AutofillProfile profile_;
  CreditCard card_;
  PhoneNumber phone_;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AutofillCapturedSitesInteractiveTest, Amazon) {
  ASSERT_TRUE(StartWebPageReplayServer("amazon"))
      << "Cannot start the local web page replay server.";

  // Navigate to the Amazon product page.
  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(),
      "window.location.href ="
      "'https://www.amazon.com/Path-Power-Years-Lyndon-Johnson/dp/"
      "0679729453/ref=sr_1_1?_encoding=UTF8&ref_=nav_ya_signin&';"));
  // Click the 'Add to Cart' button.
  ASSERT_TRUE(ClickElementBySelector("#add-to-cart-button"));
  // Click 'Proceed to Checkout'.
  ASSERT_TRUE(ClickElementBySelector("#hlb-ptc-btn-native"));
  // Test Autofilling the shipping address.
  AutofillTarget shipping_addr_autofill_targets[] = {
      {"#enterAddressFullName", "NAME_FULL", "Milton C. Waddams"},
      {"#enterAddressAddressLine1", "ADDRESS_HOME_LINE1",
       "4120 Freidrich Lane"},
      {"#enterAddressAddressLine2", "ADDRESS_HOME_LINE2", "Apt 8"},
      {"#enterAddressCity", "ADDRESS_HOME_CITY", "Austin"},
      {"#enterAddressStateOrRegion", "ADDRESS_HOME_STATE", "Texas"},
      {"#enterAddressPostalCode", "ADDRESS_HOME_ZIP", "78744"},
      {"#enterAddressCountryCode", "ADDRESS_HOME_COUNTRY", "US", true},
      {"#enterAddressPhoneNumber", "PHONE_HOME_CITY_AND_NUMBER", "5125551234"}};
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(shipping_addr_autofill_targets),
                                  std::end(shipping_addr_autofill_targets)),
      "#enterAddressFullName"));
  // Click the 'Delivery to this address' button.
  ASSERT_TRUE(ClickElementBySelector(".submit-button-with-name"));
  // Wait for the spinner to disappear.
  std::vector<std::string> state_assertions;
  state_assertions.push_back(
      "return document.querySelector(`#spinner-anchor`)"
      ".style.display == 'none';");
  ASSERT_TRUE(WaitForStateChange(state_assertions));
  // Click the 'Continue' button on the shipping page.
  ASSERT_TRUE(ClickElementBySelector(".continue-button input[type='submit']"));
  // Test autofilling the credit card info.
  AutofillTarget credit_card_autofill_targets[] = {
      {"#ccName", "CREDIT_CARD_NAME_FULL",
       base::UTF16ToASCII(credit_card()->GetRawInfo(CREDIT_CARD_NAME_FULL))},
      {"#addCreditCardNumber", "CREDIT_CARD_NUMBER",
       base::UTF16ToASCII(credit_card()->GetRawInfo(CREDIT_CARD_NUMBER))},
      // TODO(uwyiming) on m67 the month and year fields are not autofilled.
      {"#ccMonth", "CREDIT_CARD_EXP_MONTH",
       std::to_string(credit_card()->expiration_month())},
      {"#ccYear", "CREDIT_CARD_EXP_4_DIGIT_YEAR",
       base::UTF16ToASCII(
           credit_card()->GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR))}};
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(credit_card_autofill_targets),
                                  std::end(credit_card_autofill_targets)),
      "#ccName"));
}

IN_PROC_BROWSER_TEST_F(AutofillCapturedSitesInteractiveTest, Zappos) {
  ASSERT_TRUE(StartWebPageReplayServer("zappos"))
      << "Cannot start the local web page replay server.";

  // Navigate to the Zappos cart page.
  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(),
      "window.location.href = 'https://www.zappos.com/marty/cart';"));
  // Click the 'Procceed to Checkout' button.
  ASSERT_TRUE(ClickElementBySelector("._2-OrP"));
  // Click the 'Ship to a new address' button.
  ASSERT_TRUE(ClickElementBySelector(".add-address-block"));
  // Test autofilling the shipping address.
  AutofillTarget shipping_addr_autofill_targets[] = {
      {"#AddressForm_NAME", "NAME_FULL", "Milton C. Waddams"},
      {"#AddressForm_ADDRESS_LINE_1", "ADDRESS_HOME_LINE1",
       "4120 Freidrich Lane"},
      {"#AddressForm_ADDRESS_LINE_2", "ADDRESS_HOME_LINE2", "Apt 8"},
      {"#AddressForm_CITY", "ADDRESS_HOME_CITY", "Austin"},
      {"#AddressForm_STATE_OR_REGION", "ADDRESS_HOME_STATE", "Texas"},
      {"#AddressForm_ZIP_CODE", "ADDRESS_HOME_ZIP", "78744"},
      {"[name = 'AddressForm_COUNTRY']", "ADDRESS_HOME_COUNTRY", "US", true},
      {"#AddressForm_PHONE_NUMBER", "PHONE_HOME_CITY_AND_NUMBER",
       "5125551234"}};
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(shipping_addr_autofill_targets),
                                  std::end(shipping_addr_autofill_targets)),
      "#AddressForm_NAME"));
  // Click the 'Ship to this address' button.
  ASSERT_TRUE(ClickElementBySelector("#saveAndShipToThisAddress"));
  // Test autofilling the credit card info.
  AutofillTarget credit_card_autofill_targets[] = {
      {"#ccNumber", "CREDIT_CARD_NUMBER",
       base::UTF16ToASCII(credit_card()->GetRawInfo(CREDIT_CARD_NUMBER))},
      {"#ccName", "CREDIT_CARD_NAME_FULL",
       base::UTF16ToASCII(credit_card()->GetRawInfo(CREDIT_CARD_NAME_FULL))},
      {"select[id = 'ccMonth']", "CREDIT_CARD_EXP_MONTH",
       std::to_string(credit_card()->expiration_month())},
      {"select[id = 'ccYear']", "CREDIT_CARD_EXP_4_DIGIT_YEAR",
       base::UTF16ToASCII(
           credit_card()->GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR))}};
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(credit_card_autofill_targets),
                                  std::end(credit_card_autofill_targets)),
      "#ccNumber"));
}

IN_PROC_BROWSER_TEST_F(AutofillCapturedSitesInteractiveTest, Ebay) {
  ASSERT_TRUE(StartWebPageReplayServer("ebay"))
      << "Cannot start the local web page replay server.";
  std::vector<std::string> state_assertions;

  // Navigate to the ebay product page.
  ASSERT_TRUE(
      content::ExecuteScript(GetWebContents(),
                             "window.location.href = 'https://www.ebay.com/"
                             "itm/Means-of-Ascent-The-Years-of-Lyndon-Johnson"
                             "-II-by-Robert-A-Caro/302335419820?epid=245345"
                             "&hash=item46649865ac:g:XzcAAOSwuMZZL65R';"));
  // Click the 'Buy It Now' button.
  ASSERT_TRUE(ClickElementBySelector("#binBtn_btn"));
  // Click 'Continue as a guest'.
  ASSERT_TRUE(ClickElementBySelector("#gtChk"));
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(5));
  // Test autofilling the shipping address.
  AutofillTarget shipping_addr_autofill_targets[] = {
      {"#af-first-name", "HTML_TYPE_GIVEN_NAME", "Milton"},
      {"#af-last-name", "HTML_TYPE_FAMILY_NAME", "Waddams"},
      {"#af-address1", "HTML_TYPE_ADDRESS_LINE1", "4120 Freidrich Lane"},
      {"#af-address2", "HTML_TYPE_ADDRESS_LINE2", "Apt 8"},
      {"#af-city", "HTML_TYPE_ADDRESS_LEVEL2", "Austin"},
      {"#af-state", "HTML_TYPE_ADDRESS_LEVEL1", "TX", true},
      {"#af-zip", "HTML_TYPE_POSTAL_CODE", "78744"},
      {".ipt-phone", "HTML_TYPE_TEL", "(512) 555-1234"}};
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(shipping_addr_autofill_targets),
                                  std::end(shipping_addr_autofill_targets)),
      "#af-first-name"));
  // Click the 'Done' button.
  ASSERT_TRUE(ClickElementBySelector(".af-save-btn button"));
  // Select the 'Credit or debit card' option.
  // Have to wait for the shipping address to be committed first.
  state_assertions.clear();
  state_assertions.push_back(
      "return automation_helper"
      "           .isElementWithSelectorReady(`#sa-change-link`);");
  ASSERT_TRUE(
      WaitForStateChange(state_assertions, base::TimeDelta::FromSeconds(20)));
  ASSERT_TRUE(ClickElementBySelector("#CC"));
  // Test autofilling the credit card info.
  AutofillTarget credit_card_autofill_targets[] = {
      {"#cardNumber", "HTML_TYPE_CREDIT_CARD_NUMBER",
       base::UTF16ToASCII(credit_card()->GetRawInfo(CREDIT_CARD_NUMBER))},
      // TODO(uwyiming) this is broken on m67, the credit card expiration year
      // is always 2020
      {"#cardExp", "HTML_TYPE_CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR",
       base::StringPrintf(
           "%s / %s",
           base::UTF16ToASCII(credit_card()->GetRawInfo(CREDIT_CARD_EXP_MONTH))
               .c_str(),
           base::UTF16ToASCII(
               credit_card()->GetRawInfo(CREDIT_CARD_EXP_2_DIGIT_YEAR))
               .c_str())}};
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(credit_card_autofill_targets),
                                  std::end(credit_card_autofill_targets)),
      "#cardNumber"));
}

IN_PROC_BROWSER_TEST_F(AutofillCapturedSitesInteractiveTest, Apple) {
  ASSERT_TRUE(StartWebPageReplayServer("apple"))
      << "Cannot start the local web page replay server.";

  // Navigate to the Apple bag/cart page.
  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(),
      "window.location.href = 'https://www.apple.com/shop/bag';"));

  // Click the 'Check out' button.
  ASSERT_TRUE(ClickElementBySelector("#cart-actions-checkout",
                                     base::TimeDelta::FromSeconds(10)));
  // Click the 'Continue as Guest' button.
  ASSERT_TRUE(ClickElementBySelector("#guest-checkout"));
  // Click the 'Continue' button.
  ASSERT_TRUE(ClickElementBySelector("#cart-continue-button"));
  // The city, state and zip fields are initially determined by the
  // browser's geo location. Clear this field by selecting the 'Other'
  // option in the city and state drop down element.
  ASSERT_TRUE(SelectDropDownOption("#shipping-user-lookup-options", 1));
  // Test autofilling the shipping address.
  AutofillTarget shipping_addr_autofill_targets[] = {
      {"#shipping-user-firstName", "NAME_FIRST", "Milton"},
      {"#shipping-user-lastName", "NAME_LAST", "Waddams"},
      {"#shipping-user-companyName", "COMPANY_NAME", "Initech"},
      {"#shipping-user-daytimePhoneAreaCode", "PHONE_HOME_CITY_CODE", "512"},
      {"#shipping-user-daytimePhone", "PHONE_HOME_NUMBER", "5551234"},
      {"#shipping-user-street", "ADDRESS_HOME_LINE1", "4120 Freidrich Lane"},
      {"#shipping-user-street2", "ADDRESS_HOME_LINE2", "Apt 8"},
      {"#shipping-user-emailAddress", "EMAIL_ADDRESS",
       "red.swingline@initech.com"},
      // TODO(uwyiming) This is broken on m67 currently.
      //{"#shipping-user-postalCode", "ADDRESS_HOME_ZIP", "78744"},
      {"#shipping-user-state", "ADDRESS_HOME_STATE", "TX", true},
      {"#shipping-user-city", "ADDRESS_HOME_CITY", "Austin"}
      // TODO(uwyiming) This is broken on m67 currently. Autofill does not
      // fill in the phone number.
      //{"#shipping-user-mobilePhoneAreaCode", "PHONE_HOME_CITY_CODE",
      // base::UTF16ToASCII(profile()->GetRawInfo(ADDRESS_HOME_ZIP))},
      //{"#shipping-user-mobilePhone", "PHONE_HOME_NUMBER",
      // base::UTF16ToASCII(profile()->GetRawInfo(ADDRESS_HOME_ZIP))}
  };
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(shipping_addr_autofill_targets),
                                  std::end(shipping_addr_autofill_targets)),
      "#shipping-user-firstName"));
  // Click the 'Continue' button.
  ASSERT_TRUE(ClickElementBySelector("#shipping-continue-button"));
  // Select the matching address.
  ASSERT_TRUE(ClickElementBySelector("#address-verification-select-matched"));
  // Test autofilling the billing address.
  AutofillTarget billing_addr_autofill_targets[] = {
      {"#payment-credit-user-address-firstName", "NAME_FIRST", "Milton"},
      {"#payment-credit-user-address-lastName", "NAME_LAST", "Waddams"},
      {"#payment-credit-user-address-daytimePhoneAreaCode",
       "PHONE_HOME_CITY_CODE", "512"},
      {"#payment-credit-user-address-daytimePhone", "PHONE_HOME_NUMBER",
       "5551234"},
      {"#payment-credit-user-address-emailAddress", "EMAIL_ADDRESS",
       "red.swingline@initech.com"},
      {"#payment-credit-user-address-companyName", "COMPANY_NAME", "Initech"},
      {"#payment-credit-user-address-street", "ADDRESS_HOME_LINE1",
       "4120 Freidrich Lane"},
      {"#payment-credit-user-address-street2", "ADDRESS_HOME_LINE2", "Apt 8"},
      {"#payment-credit-user-address-postalCode", "ADDRESS_HOME_ZIP", "78744"},
      {"#payment-credit-user-country-selector-countryCode",
       "ADDRESS_HOME_COUNTRY", "US"}};
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(billing_addr_autofill_targets),
                                  std::end(billing_addr_autofill_targets)),
      "#payment-credit-user-address-firstName"));
  // Test autofilling the credit card.
  AutofillTarget credit_card_autofill_targets[] = {
      {"#payment-credit-method-cc0-cardNumber", "CREDIT_CARD_NUMBER",
       base::UTF16ToASCII(credit_card()->GetRawInfo(CREDIT_CARD_NUMBER))},
      {"#payment-credit-method-cc0-expirationMonth", "CREDIT_CARD_EXP_MONTH",
       base::UTF16ToASCII(credit_card()->GetRawInfo(CREDIT_CARD_EXP_MONTH))},
      {"#payment-credit-method-cc0-expirationYear",
       "CREDIT_CARD_EXP_4_DIGIT_YEAR",
       base::UTF16ToASCII(
           credit_card()->GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR))},
  };
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(credit_card_autofill_targets),
                                  std::end(credit_card_autofill_targets)),
      "#payment-credit-method-cc0-cardNumber"));
}

IN_PROC_BROWSER_TEST_F(AutofillCapturedSitesInteractiveTest, Walmart) {
  ASSERT_TRUE(StartWebPageReplayServer("walmart"))
      << "Cannot start the local web page replay server.";
  std::vector<std::string> state_assertions;

  // Navigate to the Walmart customer sign in page.
  ASSERT_TRUE(content::ExecuteScript(GetWebContents(),
                                     "window.location.href = "
                                     "'https://www.walmart.com/checkout/#/"
                                     "sign-in';"));

  // Press the 'Continue As Guest' button.
  ASSERT_TRUE(ClickElementBySelector(
      "button[data-automation-id='new-guest-continue-button']"));
  state_assertions.clear();
  state_assertions.push_back(
      "return automation_helper.isElementWithSelectorReady("
      "    `[aria-label='Select your shipping option']`);");
  state_assertions.push_back(
      "return automation_helper.isElementWithSelectorReady("
      "    `[aria-label='Select your pickup option']`);");
  state_assertions.push_back(
      "return automation_helper.isElementWithSelectorReady("
      "    `label[data-automation-id='shipMethod-EXPEDITED']`);");
  state_assertions.push_back(
      "return automation_helper.isElementWithSelectorReady("
      "    `.persistent-order-summary`);");
  // Press the 'Continue' button.
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(5));
  ASSERT_TRUE(ClickElementBySelector(
      "button[aria-label='Continue to Shipping Address']"));

  // Test autofilling the shipping address.
  AutofillTarget shipping_addr_autofill_targets[] = {
      {"[name='firstName']", "NAME_FIRST", "Milton"},
      {"[name='lastName']", "NAME_LAST", "Waddams"},
      {"[name='phone']", "PHONE_HOME_CITY_AND_NUMBER", "5125551234"},
      {"[name='email']", "EMAIL_ADDRESS", "red.swingline@initech.com"},
      {"[name='addressLineOne']", "ADDRESS_HOME_LINE1", "4120 Freidrich Lane"},
      {"[name='addressLineTwo']", "ADDRESS_HOME_LINE2", "Apt 8"},
      {"[name='city']", "ADDRESS_HOME_CITY", "Austin"},
      {"[name='state']", "ADDRESS_HOME_STATE", "TX", true},
      {"[name='postalCode']", "ADDRESS_HOME_ZIP", "78744"}};
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(shipping_addr_autofill_targets),
                                  std::end(shipping_addr_autofill_targets)),
      "[name='firstName']"));

  // Press the 'Continue' button.
  ASSERT_TRUE(
      ClickElementBySelector("[aria-label='Continue to Payment Options']"));

  state_assertions.clear();
  state_assertions.push_back(
      "return automation_helper.isElementWithSelectorReady("
      "  `[data-automation-id='address-validation-message-address']`);");
  ASSERT_TRUE(WaitForStateChange(state_assertions));

  // Press the 'Use Address Provided' button.
  ASSERT_TRUE(ClickElementBySelector(
      "[data-automation-id='address-validation-message-save-address']"));

  // Test autofilling the credit card.
  AutofillTarget credit_card_autofill_targets[] = {
      {"[name='creditCard']", "HTML_TYPE_CREDIT_CARD_NUMBER",
       base::UTF16ToASCII(credit_card()->GetRawInfo(CREDIT_CARD_NUMBER))},
      {"#month-chooser", "HTML_TYPE_CREDIT_CARD_EXP_MONTH",
       base::UTF16ToASCII(credit_card()->GetRawInfo(CREDIT_CARD_EXP_MONTH))},
      {"#year-chooser", "HTML_TYPE_CREDIT_CARD_EXP_YEAR",
       base::UTF16ToASCII(
           credit_card()->GetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR))},
  };
  ASSERT_TRUE(TestAutofill(
      std::vector<AutofillTarget>(std::begin(credit_card_autofill_targets),
                                  std::end(credit_card_autofill_targets)),
      "[name='creditCard']"));
}

}  // namespace autofill