// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/policy_tool_ui_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

class PolicyToolUITest : public InProcessBrowserTest {
 public:
  PolicyToolUITest();
  ~PolicyToolUITest() override;

 protected:
  // InProcessBrowserTest implementation.
  void SetUpInProcessBrowserTestFixture() override;
  void SetUp() override;

  base::FilePath GetSessionsDir();
  base::FilePath::StringType GetDefaultSessionName();
  base::FilePath::StringType GetSessionExtension();
  base::FilePath GetSessionPath(const base::FilePath::StringType& session_name);

  void LoadSession(const std::string& session_name);
  void DeleteSession(const std::string& session_name);

  std::unique_ptr<base::DictionaryValue> ExtractPolicyValues(bool need_status);

  bool IsInvalidSessionNameErrorMessageDisplayed();

  std::unique_ptr<base::ListValue> ExtractSessionsList();

  void CreateMultipleSessionFiles(int count);

  // Check if the error message that replaces the element is shown correctly.
  // Returns 1 if error message is shown, -1 if the error message is not shown,
  // and 0 if the displaying is incorrect (e.g. both error message and the
  // element are shown).
  int GetElementDisabledState(const std::string& element_id,
                              const std::string& error_message_id);
  std::string ExtractSinglePolicyValue(const std::string& policy_name);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(PolicyToolUITest);
};

base::FilePath PolicyToolUITest::GetSessionsDir() {
  base::FilePath profile_dir;
  EXPECT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &profile_dir));
  return profile_dir.AppendASCII(TestingProfile::kTestUserProfileDir)
      .Append(PolicyToolUIHandler::kPolicyToolSessionsDir);
}

base::FilePath::StringType PolicyToolUITest::GetDefaultSessionName() {
  return PolicyToolUIHandler::kPolicyToolDefaultSessionName;
}

base::FilePath::StringType PolicyToolUITest::GetSessionExtension() {
  return PolicyToolUIHandler::kPolicyToolSessionExtension;
}

base::FilePath PolicyToolUITest::GetSessionPath(
    const base::FilePath::StringType& session_name) {
  return GetSessionsDir()
      .Append(session_name)
      .AddExtension(GetSessionExtension());
}

PolicyToolUITest::PolicyToolUITest() {}

PolicyToolUITest::~PolicyToolUITest() {}

void PolicyToolUITest::SetUpInProcessBrowserTestFixture() {}

void PolicyToolUITest::SetUp() {
  scoped_feature_list_.InitAndEnableFeature(features::kPolicyTool);
  InProcessBrowserTest::SetUp();
}

void PolicyToolUITest::LoadSession(const std::string& session_name) {
  const std::string javascript = "$('session-name-field').value = '" +
                                 session_name +
                                 "';"
                                 "$('load-session-button').click();";
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(), javascript));
  content::RunAllTasksUntilIdle();
}

void PolicyToolUITest::DeleteSession(const std::string& session_name) {
  const std::string javascript = "$('session-list').value = '" + session_name +
                                 "';"
                                 "$('delete-session-button').click();";
  EXPECT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents(), javascript));
  content::RunAllTasksUntilIdle();
}

std::unique_ptr<base::DictionaryValue> PolicyToolUITest::ExtractPolicyValues(
    bool need_status) {
  std::string javascript =
      "var entries = document.querySelectorAll("
      "    'section.policy-table-section > * > tbody');"
      "var policies = {'chromePolicies': {}};"
      "for (var i = 0; i < entries.length; ++i) {"
      "  if (entries[i].hidden)"
      "    continue;"
      "  var name = entries[i].getElementsByClassName('name-column')[0]"
      "                       .getElementsByTagName('div')[0].textContent;"
      "  var valueContainer = entries[i].getElementsByClassName("
      "                                   'value-container')[0];"
      "  var value = valueContainer.getElementsByClassName("
      "                             'value')[0].textContent;";
  if (need_status) {
    javascript +=
        "  var status = entries[i].getElementsByClassName('status-column')[0]"
        "                         .getElementsByTagName('div')[0].textContent;"
        "  policies.chromePolicies[name] = {'value': value, 'status': status};";
  } else {
    javascript += "  policies.chromePolicies[name] = {'value': value};";
  }
  javascript += "} domAutomationController.send(JSON.stringify(policies));";
  std::string json;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(), javascript, &json));
  return base::DictionaryValue::From(base::JSONReader::Read(json));
}

std::unique_ptr<base::ListValue> PolicyToolUITest::ExtractSessionsList() {
  std::string javascript =
      "var list = $('session-list');"
      "var sessions = [];"
      "for (var i = 0; i < list.length; i++) {"
      "  sessions.push(list[i].value);"
      "}"
      "domAutomationController.send(JSON.stringify(sessions));";
  std::string json;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(), javascript, &json));
  return base::ListValue::From(base::JSONReader::Read(json));
}

bool PolicyToolUITest::IsInvalidSessionNameErrorMessageDisplayed() {
  const std::string javascript =
      "domAutomationController.send($('invalid-session-name-error')."
      "offsetWidth > "
      "0)";
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  bool result = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(contents, javascript, &result));
  return result;
}

void PolicyToolUITest::CreateMultipleSessionFiles(int count) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::CreateDirectory(GetSessionsDir()));
  base::DictionaryValue contents;
  base::Time initial_time = base::Time::Now();
  for (int i = 0; i < count; ++i) {
    contents.SetPath({"chromePolicies", "SessionId", "value"},
                     base::Value(base::IntToString(i)));
    base::FilePath::StringType session_name =
        base::FilePath::FromUTF8Unsafe(base::IntToString(i)).value();
    std::string stringified_contents;
    base::JSONWriter::Write(contents, &stringified_contents);
    base::WriteFile(GetSessionPath(session_name), stringified_contents.c_str(),
                    stringified_contents.size());
    base::Time current_time =
        initial_time - base::TimeDelta::FromSeconds(count - i);
    base::TouchFile(GetSessionPath(session_name), current_time, current_time);
  }
}

int PolicyToolUITest::GetElementDisabledState(
    const std::string& element_id,
    const std::string& error_message_id) {
  const std::string javascript =
      "var element = $('" + element_id +
      "');"
      "var errorMessage = $('" +
      error_message_id +
      "');"
      "domAutomationController.send((element.offsetWidth == 0) -"
      "                             (errorMessage.offsetWidth == 0));";
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int result = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(contents, javascript, &result));
  return result;
}

std::string PolicyToolUITest::ExtractSinglePolicyValue(
    const std::string& session_name) {
  std::unique_ptr<base::DictionaryValue> page_contents =
      ExtractPolicyValues(/*need_status=*/false);
  if (!page_contents)
    return "";

  base::Value* value =
      page_contents->FindPath({"chromePolicies", session_name, "value"});
  EXPECT_TRUE(value);
  EXPECT_EQ(base::Value::Type::STRING, value->type());
  return value->GetString();
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, CreatingSessionFiles) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Check that the directory is not created yet.
  EXPECT_FALSE(PathExists(GetSessionsDir()));

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));

  // Check that the default session file was created.
  EXPECT_TRUE(PathExists(GetSessionPath(GetDefaultSessionName())));

  // Check that when moving to a different session the corresponding file is
  // created.
  LoadSession("test_creating_sessions");
  EXPECT_TRUE(
      PathExists(GetSessionPath(FILE_PATH_LITERAL("test_creating_sessions"))));

  // Check that unicode characters are valid for the session name.
  LoadSession("сессия");
  EXPECT_TRUE(PathExists(GetSessionPath(FILE_PATH_LITERAL("сессия"))));
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, ImportingSession) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));

  // Set up policy values and put them in the session file.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::DictionaryValue test_policies;
  // Add a known chrome policy.
  test_policies.SetString("chromePolicies.AllowDinosaurEasterEgg.value",
                          "true");
  // Add an unknown chrome policy.
  test_policies.SetString("chromePolicies.UnknownPolicy.value", "test");

  std::string json;
  base::JSONWriter::WriteWithOptions(
      test_policies, base::JSONWriter::Options::OPTIONS_PRETTY_PRINT, &json);
  base::WriteFile(GetSessionPath(FILE_PATH_LITERAL("test_session")),
                  json.c_str(), json.size());

  // Import the created session and wait until all the tasks are finished.
  LoadSession("test_session");

  std::unique_ptr<base::DictionaryValue> values = ExtractPolicyValues(false);
  EXPECT_EQ(test_policies, *values);
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, Editing) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));

  // Change one policy value and get its name.
  std::string javascript =
      "document.getElementById('show-unset').click();"
      "var policyEntry = document.querySelectorAll("
      "    'section.policy-table-section > * > tbody')[0];"
      "policyEntry.getElementsByClassName('edit-button')[0].click();"
      "policyEntry.getElementsByClassName('value-edit-field')[0].value ="
      "                                                           'test';"
      "policyEntry.getElementsByClassName('save-button')[0].click();"
      "document.getElementById('show-unset').click();"
      "var name = policyEntry.getElementsByClassName('name-column')[0]"
      "                      .getElementsByTagName('div')[0].textContent;"
      "domAutomationController.send(name);";
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string name = "";
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractString(contents, javascript, &name));

  std::unique_ptr<base::Value> values = ExtractPolicyValues(true);
  base::DictionaryValue expected;
  expected.SetString("chromePolicies." + name + ".value", "test");
  expected.SetString("chromePolicies." + name + ".status", "OK");
  EXPECT_EQ(expected, *values);

  // Check if the session file is correct.
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_contents;
  base::ReadFileToString(GetSessionPath(GetDefaultSessionName()),
                         &file_contents);
  values = base::JSONReader::Read(file_contents);
  expected.SetDictionary("extensionPolicies",
                         base::MakeUnique<base::DictionaryValue>());
  expected.Remove("chromePolicies." + name + ".status", nullptr);
  EXPECT_EQ(expected, *values);

  // Set value of this variable to "", so that it becomes unset.
  javascript =
      "var policyEntry = document.querySelectorAll("
      "    'section.policy-table-section > * > tbody')[0];"
      "policyEntry.getElementsByClassName('edit-button')[0].click();"
      "policyEntry.getElementsByClassName('value-edit-field')[0].value = '';"
      "policyEntry.getElementsByClassName('save-button')[0].click();";
  EXPECT_TRUE(ExecuteScript(contents, javascript));
  values = ExtractPolicyValues(false);
  expected.Remove("chromePolicies." + name, nullptr);
  expected.Remove("extensionPolicies", nullptr);
  EXPECT_EQ(expected, *values);
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, InvalidSessionName) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  EXPECT_FALSE(IsInvalidSessionNameErrorMessageDisplayed());
  LoadSession("../test");
  EXPECT_TRUE(IsInvalidSessionNameErrorMessageDisplayed());
  LoadSession("policy");
  EXPECT_FALSE(IsInvalidSessionNameErrorMessageDisplayed());
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, InvalidJson) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  EXPECT_EQ(GetElementDisabledState("main-section", "disable-editing-error"),
            -1);
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::WriteFile(
      GetSessionsDir().Append(FILE_PATH_LITERAL("test_session.json")), "{", 1);
  LoadSession("test_session");
  EXPECT_EQ(GetElementDisabledState("main-section", "disable-editing-error"),
            1);
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, SavingToDiskError) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  EXPECT_EQ(GetElementDisabledState("session-choice", "saving"), -1);
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::DeleteFile(GetSessionsDir(), true);
  base::File not_directory(GetSessionsDir(), base::File::Flags::FLAG_CREATE |
                                                 base::File::Flags::FLAG_WRITE);
  not_directory.Close();
  LoadSession("policy");
  EXPECT_EQ(GetElementDisabledState("session-choice", "saving"), 1);
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, DefaultSession) {
  // Navigate to the tool to make sure the sessions directory is created.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));

  // Check that if there are no sessions, a session with default name is
  // created.
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_TRUE(base::PathExists(GetSessionPath(GetDefaultSessionName())));

  // Create a session file and load it.
  base::DictionaryValue expected;
  expected.SetString("chromePolicies.SessionName.value", "session");
  std::string file_contents;
  base::JSONWriter::Write(expected, &file_contents);

  base::WriteFile(GetSessionPath(FILE_PATH_LITERAL("session")),
                  file_contents.c_str(), file_contents.size());
  LoadSession("session");
  std::unique_ptr<base::DictionaryValue> values = ExtractPolicyValues(false);
  EXPECT_EQ(expected, *values);

  // Open the tool in a new tab and check that it loads the newly created
  // session (which is the last used session) and not the default session.
  chrome::NewTab(browser());
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  values = ExtractPolicyValues(false);
  EXPECT_EQ(expected, *values);
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, MultipleSessionsChoice) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::CreateDirectory(GetSessionsDir());

  // Create 5 session files with different last access times and contents.
  CreateMultipleSessionFiles(5);

  // Load the page. This should load the last session.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  std::unique_ptr<base::DictionaryValue> page_contents =
      ExtractPolicyValues(false);
  base::DictionaryValue expected;
  expected.SetPath({"chromePolicies", "SessionId", "value"}, base::Value("4"));
  EXPECT_EQ(expected, *page_contents);
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, SessionsList) {
  CreateMultipleSessionFiles(5);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  content::RunAllTasksUntilIdle();
  std::unique_ptr<base::ListValue> sessions = ExtractSessionsList();
  base::ListValue expected;
  for (int i = 4; i >= 0; --i) {
    expected.GetList().push_back(base::Value(base::IntToString(i)));
  }
  EXPECT_EQ(expected, *sessions);
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, DeleteSession) {
  CreateMultipleSessionFiles(3);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  EXPECT_EQ("2", ExtractSinglePolicyValue("SessionId"));

  // Check that a non-current session is deleted correctly.
  DeleteSession("0");
  base::ListValue expected;
  expected.GetList().push_back(base::Value("2"));
  expected.GetList().push_back(base::Value("1"));
  EXPECT_EQ(expected, *ExtractSessionsList());

  // Check that a current when the current session is deleted,
  DeleteSession("2");
  expected.GetList().erase(expected.GetList().begin());
  EXPECT_EQ(expected, *ExtractSessionsList());
}
