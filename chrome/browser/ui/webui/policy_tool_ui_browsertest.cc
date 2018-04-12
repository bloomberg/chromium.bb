// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
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
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"

namespace {

const base::FilePath::CharType kPolicyToolSessionsDir[] =
    FILE_PATH_LITERAL("Policy sessions");

const base::FilePath::CharType kPolicyToolDefaultSessionName[] =
    FILE_PATH_LITERAL("policy");

const base::FilePath::CharType kPolicyToolSessionExtension[] =
    FILE_PATH_LITERAL("json");

}  // namespace

class PolicyToolUITest : public InProcessBrowserTest {
 public:
  PolicyToolUITest();
  ~PolicyToolUITest() override;

 protected:
  void SetUp() override;

  base::FilePath GetSessionsDir();
  base::FilePath::StringType GetDefaultSessionName();
  base::FilePath::StringType GetSessionExtension();
  base::FilePath GetSessionPath(const base::FilePath::StringType& session_name);

  void LoadSession(const std::string& session_name);
  void DeleteSession(const std::string& session_name);
  void RenameSession(const std::string& session_name,
                     const std::string& new_session_name);

  std::unique_ptr<base::DictionaryValue> ExtractPolicyValues(bool need_status);

  bool IsInvalidSessionNameErrorMessageDisplayed();
  bool IsSessionRenameErrorMessageDisplayed();

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
      .Append(kPolicyToolSessionsDir);
}

base::FilePath::StringType PolicyToolUITest::GetDefaultSessionName() {
  return kPolicyToolDefaultSessionName;
}

base::FilePath::StringType PolicyToolUITest::GetSessionExtension() {
  return kPolicyToolSessionExtension;
}

base::FilePath PolicyToolUITest::GetSessionPath(
    const base::FilePath::StringType& session_name) {
  return GetSessionsDir()
      .Append(session_name)
      .AddExtension(GetSessionExtension());
}

PolicyToolUITest::PolicyToolUITest() {}

PolicyToolUITest::~PolicyToolUITest() {}

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

void PolicyToolUITest::RenameSession(const std::string& session_name,
                                     const std::string& new_session_name) {
  const std::string javascript =
      base::StrCat({"$('session-list').value = '", session_name, "';",
                    "$('rename-session-button').click();",
                    "$('new-session-name-field').value = '", new_session_name,
                    "';", "$('confirm-rename-button').click();"});
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

bool PolicyToolUITest::IsSessionRenameErrorMessageDisplayed() {
  constexpr char kJavascript[] =
      "domAutomationController.send($('session-rename-error').hidden == false)";
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  bool result = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(contents, kJavascript, &result));
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

class PolicyToolUIExportTest : public PolicyToolUITest {
 public:
  PolicyToolUIExportTest();
  ~PolicyToolUIExportTest() override;

 protected:
  // InProcessBrowserTest implementation.
  void SetUpInProcessBrowserTestFixture() override;

  // The temporary directory and file path for policy saving.
  base::ScopedTempDir export_policies_test_dir_;
  base::FilePath export_policies_test_file_path_;
};

PolicyToolUIExportTest::PolicyToolUIExportTest() {}

PolicyToolUIExportTest::~PolicyToolUIExportTest() {}

void PolicyToolUIExportTest::SetUpInProcessBrowserTestFixture() {
  ASSERT_TRUE(export_policies_test_dir_.CreateUniqueTempDir());
  const std::string filename = "policy.json";
  export_policies_test_file_path_ =
      export_policies_test_dir_.GetPath().AppendASCII(filename);
}

// An artificial SelectFileDialog that immediately returns the location of test
// file instead of showing the UI file picker.
class TestSelectFileDialogPolicyTool : public ui::SelectFileDialog {
 public:
  TestSelectFileDialogPolicyTool(ui::SelectFileDialog::Listener* listener,
                                 std::unique_ptr<ui::SelectFilePolicy> policy,
                                 const base::FilePath& default_selected_path)
      : ui::SelectFileDialog(listener, std::move(policy)) {
    default_path_ = default_selected_path;
  }

  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    listener_->FileSelected(default_path_, /*index=*/0, /*params=*/nullptr);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }

  void ListenerDestroyed() override {}

  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~TestSelectFileDialogPolicyTool() override {}
  base::FilePath default_path_;
};

// A factory associated with the artificial file picker.
class TestSelectFileDialogFactoryPolicyTool
    : public ui::SelectFileDialogFactory {
 public:
  TestSelectFileDialogFactoryPolicyTool(
      const base::FilePath& default_selected_path) {
    default_path_ = default_selected_path;
  }

 private:
  base::FilePath default_path_;
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialogPolicyTool(listener, std::move(policy),
                                              default_path_);
  }
};

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

// Flaky on all platforms, see https://crbug.com/797446
IN_PROC_BROWSER_TEST_F(PolicyToolUITest, DISABLED_Editing) {
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
                         std::make_unique<base::DictionaryValue>());
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
  LoadSession("/full_path");
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

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, RenameSession) {
  CreateMultipleSessionFiles(3);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  EXPECT_EQ("2", ExtractSinglePolicyValue("SessionId"));

  // Check that a non-current session is renamed correctly.
  RenameSession("0", "4");
  EXPECT_FALSE(IsSessionRenameErrorMessageDisplayed());
  base::ListValue expected;
  expected.GetList().push_back(base::Value("2"));
  expected.GetList().push_back(base::Value("1"));
  expected.GetList().push_back(base::Value("4"));
  EXPECT_EQ(expected, *ExtractSessionsList());

  // Check that the current session can be renamed properly.
  RenameSession("2", "5");
  EXPECT_FALSE(IsSessionRenameErrorMessageDisplayed());
  expected.GetList()[0] = base::Value("5");
  EXPECT_EQ(expected, *ExtractSessionsList());
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, RenameSessionWithExistingSessionName) {
  CreateMultipleSessionFiles(3);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  EXPECT_EQ("2", ExtractSinglePolicyValue("SessionId"));

  // Check that a session can not be renamed with a name of another existing
  // session.
  RenameSession("2", "1");
  EXPECT_TRUE(IsSessionRenameErrorMessageDisplayed());
  base::ListValue expected;
  expected.GetList().push_back(base::Value("2"));
  expected.GetList().push_back(base::Value("1"));
  expected.GetList().push_back(base::Value("0"));
  EXPECT_EQ(expected, *ExtractSessionsList());
}

IN_PROC_BROWSER_TEST_F(PolicyToolUITest, RenameSessionInvalidName) {
  CreateMultipleSessionFiles(3);
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  EXPECT_EQ("2", ExtractSinglePolicyValue("SessionId"));

  RenameSession("2", "../");
  EXPECT_TRUE(IsSessionRenameErrorMessageDisplayed());
  base::ListValue expected;
  expected.GetList().push_back(base::Value("2"));
  expected.GetList().push_back(base::Value("1"));
  expected.GetList().push_back(base::Value("0"));
  EXPECT_EQ(expected, *ExtractSessionsList());

  // Check that full path is not allowed
  RenameSession("2", "/full_path");
  EXPECT_TRUE(IsSessionRenameErrorMessageDisplayed());
  EXPECT_EQ(expected, *ExtractSessionsList());
}

IN_PROC_BROWSER_TEST_F(PolicyToolUIExportTest, ExportSessionPolicyToLinux) {
  CreateMultipleSessionFiles(3);

  // Set SelectFileDialog to use our factory.
  ui::SelectFileDialog::SetFactory(new TestSelectFileDialogFactoryPolicyTool(
      export_policies_test_file_path_));

  // Test if the current session policy is successfully exported.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy-tool"));
  std::string javascript =
      "$('show-unset').click();"
      "var policyEntry = document.querySelectorAll("
      "    'section.policy-table-section > * > tbody')[0];"
      "policyEntry.getElementsByClassName('edit-button')[0].click();"
      "policyEntry.getElementsByClassName('value-edit-field')[0].value ="
      "                                                           'test';"
      "policyEntry.getElementsByClassName('save-button')[0].click();"
      "$('export-policies-linux').click()";

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(contents, javascript));

  // Because we created 3 session policies (with paths {0, 1, 2}), the last one
  // is the current active session policy.
  EXPECT_TRUE(base::ContentsEqual(export_policies_test_file_path_,
                                  GetSessionPath(FILE_PATH_LITERAL("2"))));

  // Test if after an export action, we can continue exporting.
  std::string change_session_js =
      "$('session-name-field').value = '1';"
      "$('load-session-button').click();";

  EXPECT_TRUE(content::ExecuteScript(contents, change_session_js + javascript));
  base::TaskScheduler::GetInstance()->FlushForTesting();
  EXPECT_TRUE(base::ContentsEqual(export_policies_test_file_path_,
                                  GetSessionPath(FILE_PATH_LITERAL("1"))));
  TestSelectFileDialogPolicyTool::SetFactory(nullptr);
}
