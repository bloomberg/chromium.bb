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
#include "chrome/browser/ui/javascript_dialogs/javascript_dialog_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

  void LoadSession(const std::string& session_name);

  void LoadSessionAndWaitForAlert(const std::string& session_name);

  std::unique_ptr<base::DictionaryValue> ExtractPolicyValues(bool need_status);

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(PolicyToolUITest);
};

base::FilePath PolicyToolUITest::GetSessionsDir() {
  base::FilePath profile_dir;
  EXPECT_TRUE(PathService::Get(chrome::DIR_USER_DATA, &profile_dir));
  return profile_dir.AppendASCII(TestingProfile::kTestUserProfileDir)
      .Append(FILE_PATH_LITERAL("Policy sessions"));
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

void PolicyToolUITest::LoadSessionAndWaitForAlert(
    const std::string& session_name) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  JavaScriptDialogTabHelper* js_helper =
      JavaScriptDialogTabHelper::FromWebContents(contents);
  base::RunLoop dialog_wait;
  js_helper->SetDialogShownCallbackForTesting(dialog_wait.QuitClosure());
  LoadSession(session_name);
  dialog_wait.Run();
  EXPECT_TRUE(js_helper->IsShowingDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, CreatingSessionFiles) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  // Check that the directory is not created yet.
  EXPECT_FALSE(PathExists(GetSessionsDir()));

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));

  const base::FilePath default_session_path =
      GetSessionsDir().Append(FILE_PATH_LITERAL("policy.json"));
  // Check that the default session file was created.
  EXPECT_TRUE(PathExists(default_session_path));

  // Check that when moving to a different session the corresponding file is
  // created.
  LoadSession("test_creating_sessions");

  const base::FilePath test_session_path =
      GetSessionsDir().Append(FILE_PATH_LITERAL("test_creating_sessions.json"));
  EXPECT_TRUE(PathExists(test_session_path));

  // Check that unicode characters are valid for the session name.
  LoadSession("сессия");

  const base::FilePath russian_session_path =
      GetSessionsDir().Append(FILE_PATH_LITERAL("сессия.json"));
  EXPECT_TRUE(PathExists(russian_session_path));
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, ImportingSession) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));

  // Set up policy values and put them in the session file.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::DictionaryValue test_policies;
  // Add a known chrome policy.
  test_policies.SetString("chromePolicies.AllowDinosaurEasterEgg.value",
                          "true");
  // Add an unknown chrome policy.
  test_policies.SetString("chromePolicies.UnknownPolicy.value", "test");

  std::string json;
  base::JSONWriter::WriteWithOptions(
      test_policies, base::JSONWriter::Options::OPTIONS_PRETTY_PRINT, &json);
  base::WriteFile(
      GetSessionsDir().Append(FILE_PATH_LITERAL("test_session.json")),
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
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string file_contents;
  base::ReadFileToString(
      GetSessionsDir().Append(FILE_PATH_LITERAL("policy.json")),
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

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, InvalidFilename) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  LoadSessionAndWaitForAlert("../test");
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, InvalidJson) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::WriteFile(
      GetSessionsDir().Append(FILE_PATH_LITERAL("test_session.json")), "{", 1);
  LoadSessionAndWaitForAlert("test_session");
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, UnableToCreateDirectoryOrFile) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::DeleteFile(GetSessionsDir(), true);
  base::File not_directory(GetSessionsDir(), base::File::Flags::FLAG_CREATE |
                                                 base::File::Flags::FLAG_WRITE);
  not_directory.Close();
  LoadSessionAndWaitForAlert("test_session");
}
