// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
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

namespace {

const base::TimeDelta default_action_timeout = base::TimeDelta::FromSeconds(30);
const base::TimeDelta paint_event_check_interval =
    base::TimeDelta::FromMilliseconds(500);
const int autofill_action_num_retries = 5;

// PageActivityObserver waits until Chrome finishes loading a page and stops
// making visual updates to the page.
class PageActivityObserver : public content::WebContentsObserver {
 public:
  explicit PageActivityObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}
  ~PageActivityObserver() override = default;

  // Wait until Chrome finishes loading a page and updating the page's visuals.
  // If Chrome finishes loading a page but continues to paint every half
  // second, exit after |continuous_paint_timeout| expires since Chrome
  // finished loading the page.
  void WaitTillPageIsIdle(
      base::TimeDelta continuous_paint_timeout = default_action_timeout) {
    base::TimeTicks finished_load_time = base::TimeTicks::Now();
    bool page_is_loading = false;
    do {
      paint_occurred_during_last_loop_ = false;
      base::RunLoop heart_beat;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, heart_beat.QuitClosure(), paint_event_check_interval);
      heart_beat.Run();
      page_is_loading =
          web_contents()->IsWaitingForResponse() || web_contents()->IsLoading();
      if (page_is_loading) {
        finished_load_time = base::TimeTicks::Now();
      } else if (base::TimeTicks::Now() - finished_load_time >
                 continuous_paint_timeout) {
        // |continuous_paint_timeout| has expired since Chrome loaded the page.
        // During this period of time, Chrome has been continuously painting
        // the page. In this case, the page is probably idle, but a bug, a
        // blinking caret or a persistent animation is making Chrome paint at
        // regular intervals. Exit.
        break;
      }
    } while (page_is_loading || paint_occurred_during_last_loop_);
  }

 private:
  void DidCommitAndDrawCompositorFrame() override {
    paint_occurred_during_last_loop_ = true;
  }

  bool paint_occurred_during_last_loop_ = false;

  DISALLOW_COPY_AND_ASSIGN(PageActivityObserver);
};

std::string FilePathToUTF8(const base::FilePath::StringType& str) {
#if defined(OS_WIN)
  return base::WideToUTF8(str);
#else
  return str;
#endif
}

base::FilePath GetReplayFilesDirectory() {
  base::FilePath src_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
  return src_dir.Append(
      FILE_PATH_LITERAL("chrome/test/data/autofill/captured_sites"));
}

// Iterate through Autofill's Web Page Replay capture file directory to look
// for captures sites and automation recipe files. Return a list of sites for
// which recipe-based testing is available.
std::vector<std::string> GetCapturedSites() {
  std::vector<std::string> sites;
  base::FileEnumerator capture_files(GetReplayFilesDirectory(), false,
                                     base::FileEnumerator::FILES);
  for (base::FilePath file = capture_files.Next(); !file.empty();
       file = capture_files.Next()) {
    // If a site capture file is found, also look to see if the directory has
    // a corresponding recorded action recipe log file.
    // A site capture file has no extension. A recorded action recipe log file
    // has the '.test' extension.
    if (file.Extension().empty() &&
        base::PathExists(file.AddExtension(FILE_PATH_LITERAL(".test")))) {
      sites.push_back(FilePathToUTF8(file.BaseName().value()));
    }
  }
  std::sort(sites.begin(), sites.end());
  return sites;
}

}  // namespace

namespace autofill {

class AutofillCapturedSitesInteractiveTest
    : public AutofillUiTest,
      public ::testing::WithParamInterface<std::string> {
 protected:
  AutofillCapturedSitesInteractiveTest()
      : profile_(test::GetFullProfile()),
        card_(CreditCard(base::GenerateGUID(), "http://www.example.com")) {}
  ~AutofillCapturedSitesInteractiveTest() override {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    AutofillUiTest::SetUpOnMainThread();
    SetupTestProfile();
    EXPECT_TRUE(InstallWebPageReplayServerRootCert())
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
            // Uncomment to use the live autofill prediction server.
            // "EXCLUDE clients1.google.com,"
            "EXCLUDE localhost",
            host_http_port_, host_https_port_));
  }

  bool StartWebPageReplayServer(const std::string& replay_file) {
    std::vector<std::string> args;
    base::FilePath src_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    args.push_back(base::StringPrintf("--http_port=%d", host_http_port_));
    args.push_back(base::StringPrintf("--https_port=%d", host_https_port_));
    args.push_back(base::StringPrintf(
        "--inject_scripts=%s,%s",
        FilePathToUTF8(src_dir
                           .Append(FILE_PATH_LITERAL(
                               "third_party/catapult/web_page_replay_go"))
                           .Append(FILE_PATH_LITERAL("deterministic.js"))
                           .value())
            .c_str(),
        FilePathToUTF8(
            src_dir
                .Append(FILE_PATH_LITERAL(
                    "chrome/test/data/web_page_replay_go_helper_scripts"))
                .Append(FILE_PATH_LITERAL("automation_helper.js"))
                .value())
            .c_str()));

    // Specify the replay file.
    args.push_back(base::StringPrintf(
        "%s", FilePathToUTF8(
                  GetReplayFilesDirectory().AppendASCII(replay_file).value())
                  .c_str()));

    web_page_replay_server_ = RunWebPageReplayCmd("replay", args);

    // Sleep 20 seconds to wait for the web page replay server to start.
    // TODO(bug 847910): create a process std stream reader class to use the
    // process output to determine when the server is ready.
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(20));

    return web_page_replay_server_.IsValid();
  }

  bool StopWebPageReplayServer() {
    if (web_page_replay_server_.IsValid())
      return web_page_replay_server_.Terminate(0, true);
    // The test server hasn't started, no op.
    return true;
  }

  bool ReplayTestRecipe(const char* recipe_file_name) {
    // Read the text of the recipe file.
    base::FilePath src_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    base::FilePath recipe_file_path = GetReplayFilesDirectory().AppendASCII(
        base::StringPrintf("%s.test", recipe_file_name));
    base::ThreadRestrictions::SetIOAllowed(true);
    std::string json_text;
    CHECK(base::ReadFileToString(recipe_file_path, &json_text));

    // Convert the file text into a json object.
    std::unique_ptr<base::DictionaryValue> recipe =
        base::DictionaryValue::From(base::JSONReader().ReadToValue(json_text));
    if (!recipe) {
      ADD_FAILURE() << "Failed to deserialize json text!";
      return false;
    }

    InitializeBrowserToExecuteRecipe(recipe);

    // Iterate through and execute each action in the recipe.
    base::Value* action_list_container = recipe->FindKey("actions");
    CHECK(action_list_container);
    CHECK_EQ(base::Value::Type::LIST, action_list_container->type());
    base::Value::ListStorage& action_list = action_list_container->GetList();

    for (base::ListValue::iterator it_action = action_list.begin();
         it_action != action_list.end(); ++it_action) {
      base::DictionaryValue* action;
      CHECK(it_action->GetAsDictionary(&action));

      base::Value* type_container = action->FindKey("type");
      CHECK(type_container);
      CHECK_EQ(base::Value::Type::STRING, type_container->type());
      std::string type = type_container->GetString();

      if (base::CompareCaseInsensitiveASCII(type, "waitFor") == 0) {
        ExecuteWaitForStateAction(action);
      } else if (base::CompareCaseInsensitiveASCII(type, "execute") == 0) {
        ExecuteRunCommandAction(action);
      } else if (base::CompareCaseInsensitiveASCII(type, "click") == 0) {
        ExecuteClickAction(action);
      } else if (base::CompareCaseInsensitiveASCII(type, "type") == 0) {
        ExecuteTypeAction(action);
      } else if (base::CompareCaseInsensitiveASCII(type, "select") == 0) {
        ExecuteSelectDropdownAction(action);
      } else if (base::CompareCaseInsensitiveASCII(type, "autofill") == 0) {
        ExecuteAutofillAction(action);
      } else if (base::CompareCaseInsensitiveASCII(type, "validateField") ==
                 0) {
        ExecuteValidateFieldValueAction(action);
      } else {
        ADD_FAILURE() << "Unrecognized action type: " << type;
      }
    }    // end foreach action
    return true;
  }

  const CreditCard credit_card() { return card_; }

  const AutofillProfile profile() { return profile_; }

 private:
  void SetupTestProfile() {
    test::SetCreditCardInfo(&card_, "Milton Waddams", "9621327911759602", "5",
                            "2027", "1");
    test::SetProfileInfo(&profile_, "Milton", "C.", "Waddams",
                         "red.swingline@initech.com", "Initech",
                         "4120 Freidrich Lane", "Apt 8", "Austin", "Texas",
                         "78744", "US", "5125551234");
    AddTestAutofillData(browser(), profile_, card_);
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
      const std::string& cmd,
      const std::vector<std::string>& args,
      const base::TimeDelta& timeout = base::TimeDelta::FromSeconds(5)) {
    base::Process process = RunWebPageReplayCmd(cmd, args);
    if (process.IsValid()) {
      int exit_code;
      if (process.WaitForExitWithTimeout(timeout, &exit_code))
        return (exit_code == 0);
    }
    return false;
  }

  base::Process RunWebPageReplayCmd(const std::string& cmd,
                                    const std::vector<std::string>& args) {
    base::LaunchOptions options = base::LaunchOptionsForTest();
    base::FilePath exe_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &exe_dir));
    base::FilePath web_page_replay_binary_dir =
        exe_dir.Append(FILE_PATH_LITERAL(
            "third_party/catapult/telemetry/telemetry/internal/bin"));
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
    // The capture files used in these browser tests are also used on iOS to
    // test autofill.
    // The custom cert and key files are different from those of the offical
    // WPR releases. The custom files are made to work on iOS.
    base::FilePath src_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    base::FilePath web_page_replay_support_file_dir =
        src_dir.Append(FILE_PATH_LITERAL(
            "components/test/data/autofill/web_page_replay_support_files"));
    full_command.AppendArg(base::StringPrintf(
        "--https_cert_file=%s",
        FilePathToUTF8(web_page_replay_support_file_dir
                           .Append(FILE_PATH_LITERAL("wpr_cert.pem"))
                           .value())
            .c_str()));
    full_command.AppendArg(base::StringPrintf(
        "--https_key_file=%s",
        FilePathToUTF8(web_page_replay_support_file_dir
                           .Append(FILE_PATH_LITERAL("wpr_key.pem"))
                           .value())
            .c_str()));

    for (auto const& arg : args)
      full_command.AppendArg(arg);

    return base::LaunchProcess(full_command, options);
  }

  bool WaitForStateChange(
      const std::vector<std::string>& state_assertions,
      const base::TimeDelta& timeout = default_action_timeout) {
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

  bool AllAssertionsPassed(const std::vector<std::string>& assertions)
      WARN_UNUSED_RESULT {
    for (std::string const& assertion : assertions) {
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

  bool ExecuteJavaScriptOnElementByXpath(
      const std::string& element_xpath,
      const std::string& execute_function_body,
      const base::TimeDelta& time_to_wait_for_element =
          default_action_timeout) {
    std::string js(base::StringPrintf(
        "try {"
        "  var element = automation_helper.getElementByXpath(`%s`);"
        "  (function(target) { %s })(element);"
        "} catch(ex) {}",
        element_xpath.c_str(), execute_function_body.c_str()));
    return content::ExecuteScript(GetWebContents(), js);
  }

  bool ExpectElementPropertyEquals(
      const std::string& element_xpath,
      const std::string& get_property_function_body,
      const std::string& expected_value,
      bool ignoreCase = false) {
    std::string value;
    if (content::ExecuteScriptAndExtractString(
            GetWebContents(),
            base::StringPrintf(
                "window.domAutomationController.send("
                "    (function() {"
                "      try {"
                "        var element = function() {"
                "          return automation_helper.getElementByXpath(`%s`);"
                "        }();"
                "        return function(target){%s}(element);"
                "      } catch (ex) {}"
                "      return 'Exception encountered';"
                "    })());",
                element_xpath.c_str(), get_property_function_body.c_str()),
            &value)) {
      if (ignoreCase) {
        EXPECT_TRUE(base::EqualsCaseInsensitiveASCII(expected_value, value))
            << "Field xpath: `" << element_xpath << "`, "
            << "Expected: " << expected_value << ", actual: " << value;
      } else {
        EXPECT_EQ(expected_value, value)
            << "Field xpath: `" << element_xpath << "`, ";
      }
      return true;
    }
    LOG(ERROR) << element_xpath << ", " << get_property_function_body;
    return false;
  }

  // Functions for deserializing and executing actions from the test recipe
  // JSON object.
  void InitializeBrowserToExecuteRecipe(
      std::unique_ptr<base::DictionaryValue>& recipe) {
    // Extract the starting URL from the test recipe.
    base::Value* starting_url_container = recipe->FindKey("startingURL");
    CHECK(starting_url_container);
    CHECK_EQ(base::Value::Type::STRING, starting_url_container->type());
    LOG(INFO) << "Navigating to " << starting_url_container->GetString();

    // Navigate to the starting URL, wait for the page to complete loading.
    PageActivityObserver page_activity_observer(GetWebContents());
    CHECK(content::ExecuteScript(
        GetWebContents(),
        base::StringPrintf("window.location.href = '%s';",
                           starting_url_container->GetString().c_str())));
    page_activity_observer.WaitTillPageIsIdle();
  }

  void ExecuteWaitForStateAction(base::DictionaryValue* action) {
    // Extract the list of JavaScript assertions into a vector.
    std::vector<std::string> state_assertions;
    base::Value* assertions_list_container = action->FindKey("assertions");
    CHECK(assertions_list_container);
    CHECK_EQ(base::Value::Type::LIST, assertions_list_container->type());
    base::Value::ListStorage& assertions_list =
        assertions_list_container->GetList();
    for (base::ListValue::iterator it_assertion = assertions_list.begin();
         it_assertion != assertions_list.end(); ++it_assertion) {
      CHECK_EQ(base::Value::Type::STRING, it_assertion->type());
      state_assertions.push_back(it_assertion->GetString());
    }
    LOG(INFO) << "Waiting for page to reach a state.";

    // Wait for all of the assertions to become true on the current page.
    CHECK(WaitForStateChange(state_assertions, default_action_timeout));
  }

  void ExecuteRunCommandAction(base::DictionaryValue* action) {
    // Extract the list of JavaScript commands into a vector.
    std::vector<std::string> commands;
    base::Value* commands_list_container = action->FindKey("commands");
    CHECK(commands_list_container);
    CHECK_EQ(base::Value::Type::LIST, commands_list_container->type());
    base::Value::ListStorage& commands_list =
        commands_list_container->GetList();

    for (base::ListValue::iterator it_command = commands_list.begin();
         it_command != commands_list.end(); ++it_command) {
      CHECK_EQ(base::Value::Type::STRING, it_command->type());
      commands.push_back(it_command->GetString());
    }
    LOG(INFO) << "Running JavaScript commands on the page.";

    // Execute the commands.
    PageActivityObserver page_activity_observer(GetWebContents());
    for (const std::string& command : commands) {
      CHECK(content::ExecuteScript(GetWebContents(), command));
      // Wait in case the JavaScript command triggers page load or layout
      // changes.
      page_activity_observer.WaitTillPageIsIdle();
    }
  }

  void ExecuteClickAction(base::DictionaryValue* action) {
    std::string xpath = GetTargetHTMLElementXpathFromAction(action);
    WaitForElemementToBeReady(xpath);

    LOG(INFO) << "Left mouse clicking `" << xpath << "`.";
    PageActivityObserver page_activity_observer(GetWebContents());
    CHECK(ExecuteJavaScriptOnElementByXpath(xpath, "target.click();"));
    page_activity_observer.WaitTillPageIsIdle();
  }

  void ExecuteTypeAction(base::DictionaryValue* action) {
    base::Value* value_container = action->FindKey("value");
    CHECK(value_container);
    CHECK_EQ(base::Value::Type::STRING, value_container->type());
    std::string value = value_container->GetString();

    std::string xpath = GetTargetHTMLElementXpathFromAction(action);
    WaitForElemementToBeReady(xpath);

    LOG(INFO) << "Typing '" << value << "' inside `" << xpath << "`.";
    PageActivityObserver page_activity_observer(GetWebContents());
    CHECK(ExecuteJavaScriptOnElementByXpath(
        xpath, base::StringPrintf(
                   "automation_helper.setInputElementValue(target, `%s`);",
                   value.c_str())));
    page_activity_observer.WaitTillPageIsIdle();
  }

  void ExecuteSelectDropdownAction(base::DictionaryValue* action) {
    base::Value* index_container = action->FindKey("index");
    CHECK(index_container);
    CHECK_EQ(base::Value::Type::INTEGER, index_container->type());
    int index = index_container->GetInt();

    std::string xpath = GetTargetHTMLElementXpathFromAction(action);
    WaitForElemementToBeReady(xpath);

    LOG(INFO) << "Select option '" << index << "' from `" << xpath << "`.";
    PageActivityObserver page_activity_observer(GetWebContents());
    CHECK(ExecuteJavaScriptOnElementByXpath(
        xpath, base::StringPrintf(
                   "automation_helper"
                   "  .selectOptionFromDropDownElementByIndex(target, %d);",
                   index_container->GetInt())));
    page_activity_observer.WaitTillPageIsIdle();
  }

  void ExecuteAutofillAction(base::DictionaryValue* action) {
    std::string xpath = GetTargetHTMLElementXpathFromAction(action);
    WaitForElemementToBeReady(xpath);

    LOG(INFO) << "Invoking Chrome Autofill on `" << xpath << "`.";
    PageActivityObserver page_activity_observer(GetWebContents());
    // Clear the input box first, in case a previous value is there.
    // If the text input box is not clear, pressing the down key will not
    // bring up the autofill suggestion box.
    // This can happen on sites that requires the user to sign in. After
    // signing in, the site fills the form with the user's profile
    // information.
    CHECK(ExecuteJavaScriptOnElementByXpath(
        xpath, "automation_helper.setInputElementValue(target, ``);"));
    CHECK(TryFillForm(xpath, autofill_action_num_retries));
    page_activity_observer.WaitTillPageIsIdle();
  }

  void ExecuteValidateFieldValueAction(base::DictionaryValue* action) {
    base::Value* autofill_prediction_container =
        action->FindKey("expectedAutofillType");
    CHECK(autofill_prediction_container);
    CHECK_EQ(base::Value::Type::STRING, autofill_prediction_container->type());
    std::string expected_autofill_prediction_type =
        autofill_prediction_container->GetString();

    base::Value* expected_value_container = action->FindKey("expectedValue");
    CHECK(expected_value_container);
    CHECK_EQ(base::Value::Type::STRING, expected_value_container->type());
    std::string expected_value = expected_value_container->GetString();

    std::string xpath = GetTargetHTMLElementXpathFromAction(action);
    WaitForElemementToBeReady(xpath);

    LOG(INFO) << "Checking the field `" << xpath << "`.";
    ExpectElementPropertyEquals(
        xpath.c_str(), "return target.getAttribute('autofill-prediction');",
        expected_autofill_prediction_type, true);
    ExpectElementPropertyEquals(xpath.c_str(), "return target.value;",
                                expected_value);
  }

  std::string GetTargetHTMLElementXpathFromAction(
      base::DictionaryValue* action) {
    base::Value* xpath_container = action->FindKey("selector");
    CHECK(xpath_container);
    CHECK_EQ(base::Value::Type::STRING, xpath_container->type());
    return xpath_container->GetString();
  }

  void WaitForElemementToBeReady(std::string xpath) {
    std::vector<std::string> state_assertions;
    state_assertions.push_back(base::StringPrintf(
        "return automation_helper.isElementWithXpathReady(`%s`);",
        xpath.c_str()));
    CHECK(WaitForStateChange(state_assertions, default_action_timeout));
  }

  // The Web Page Replay server that will be serving the captured sites
  base::Process web_page_replay_server_;
  const int host_http_port_ = 8080;
  const int host_https_port_ = 8081;

  AutofillProfile profile_;
  CreditCard card_;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(AutofillCapturedSitesInteractiveTest, Recipe) {
  // Prints the path of the test to be executed.
  LOG(INFO) << GetParam();
  ASSERT_TRUE(StartWebPageReplayServer(GetParam()));
  ASSERT_TRUE(ReplayTestRecipe(GetParam().c_str()));
}

struct GetParamAsString {
  template <class ParamType>
  std::string operator()(const testing::TestParamInfo<ParamType>& info) const {
    return info.param;
  }
};

INSTANTIATE_TEST_CASE_P(,
                        AutofillCapturedSitesInteractiveTest,
                        testing::ValuesIn(GetCapturedSites()),
                        GetParamAsString());
}  // namespace autofill
