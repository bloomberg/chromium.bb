// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "url/gurl.h"

using testing::Return;
using testing::_;

namespace {

std::vector<std::string> PopulateExpectedPolicy(
    const std::string& name,
    const std::string& value,
    const std::string& source,
    const policy::PolicyMap::Entry* metadata,
    bool unknown) {
  std::vector<std::string> expected_policy;

  // Populate expected scope.
  if (metadata) {
    expected_policy.push_back(l10n_util::GetStringUTF8(
        metadata->scope == policy::POLICY_SCOPE_MACHINE ?
            IDS_POLICY_SCOPE_DEVICE : IDS_POLICY_SCOPE_USER));
  } else {
    expected_policy.push_back(std::string());
  }

  // Populate expected level.
  if (metadata) {
    expected_policy.push_back(l10n_util::GetStringUTF8(
        metadata->level == policy::POLICY_LEVEL_RECOMMENDED ?
            IDS_POLICY_LEVEL_RECOMMENDED : IDS_POLICY_LEVEL_MANDATORY));
  } else {
    expected_policy.push_back(std::string());
  }
  // Populate expected source name.
  expected_policy.push_back(source);

  // Populate expected policy name.
  expected_policy.push_back(name);

  // Populate expected policy value.
  expected_policy.push_back(value);

  // Populate expected status.
  if (unknown)
    expected_policy.push_back(l10n_util::GetStringUTF8(IDS_POLICY_UNKNOWN));
  else if (metadata)
    expected_policy.push_back(l10n_util::GetStringUTF8(IDS_POLICY_OK));
  else
    expected_policy.push_back(l10n_util::GetStringUTF8(IDS_POLICY_UNSET));

  // Populate expected expanded policy value.
  expected_policy.push_back(value);

  return expected_policy;
}

void SetExpectedPolicy(base::DictionaryValue* expected,
                       const std::string& name,
                       const std::string& level,
                       const std::string& scope,
                       const std::string& source,
                       const base::Value& value) {
  const char prefix[] = "chromePolicies";
  expected->SetPath({prefix, name.c_str(), "level"}, base::Value(level));
  expected->SetPath({prefix, name.c_str(), "scope"}, base::Value(scope));
  expected->SetPath({prefix, name.c_str(), "source"}, base::Value(source));
  expected->SetPath({prefix, name.c_str(), "value"}, value.Clone());
}

// The temporary directory and file paths for policy saving.
base::ScopedTempDir export_policies_test_dir;
base::FilePath export_policies_test_file_path;

}  // namespace

class PolicyUITest : public InProcessBrowserTest {
 public:
  PolicyUITest();
  ~PolicyUITest() override;

 protected:
  // InProcessBrowserTest implementation.
  void SetUpInProcessBrowserTestFixture() override;

  void UpdateProviderPolicy(const policy::PolicyMap& policy);

  void VerifyPolicies(const std::vector<std::vector<std::string> >& expected);

  void VerifyExportingPolicies(const base::DictionaryValue& expected);

 protected:
  policy::MockConfigurationPolicyProvider provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyUITest);
};

// An artificial SelectFileDialog that immediately returns the location of test
// file instead of showing the UI file picker.
class TestSelectFileDialog : public ui::SelectFileDialog {
 public:
  TestSelectFileDialog(ui::SelectFileDialog::Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)) {}

  void SelectFileImpl(Type type,
                      const base::string16& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params) override {
    listener_->FileSelected(export_policies_test_file_path, 0, nullptr);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }

  void ListenerDestroyed() override {}

  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  ~TestSelectFileDialog() override {}
};

// A factory associated with the artificial file picker.
class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 private:
  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new TestSelectFileDialog(listener, std::move(policy));
  }
};

PolicyUITest::PolicyUITest() {
}

PolicyUITest::~PolicyUITest() {
}

void PolicyUITest::SetUpInProcessBrowserTestFixture() {
  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);

  // Create a directory for testing exporting policies.
  ASSERT_TRUE(export_policies_test_dir.CreateUniqueTempDir());
  const std::string filename = "policy.json";
  export_policies_test_file_path =
      export_policies_test_dir.GetPath().AppendASCII(filename);
}

void PolicyUITest::UpdateProviderPolicy(const policy::PolicyMap& policy) {
  provider_.UpdateChromePolicy(policy);
  base::RunLoop loop;
  loop.RunUntilIdle();
}

void PolicyUITest::VerifyPolicies(
    const std::vector<std::vector<std::string> >& expected_policies) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy"));

  // Retrieve the text contents of the policy table cells for all policies.
  const std::string javascript =
      "var entries = document.querySelectorAll("
      "    'section.policy-table-section > * > tbody');"
      "var policies = [];"
      "for (var i = 0; i < entries.length; ++i) {"
      "  var items = entries[i].querySelectorAll('tr > td');"
      "  var values = [];"
      "  for (var j = 0; j < items.length; ++j) {"
      "    var item = items[j];"
      "    var children = item.getElementsByTagName('div');"
      "    if (children.length == 1)"
      "      item = children[0];"
      "    children = item.getElementsByTagName('span');"
      "    if (children.length == 1)"
      "      item = children[0];"
      "    values.push(item.textContent);"
      "  }"
      "  policies.push(values);"
      "}"
      "domAutomationController.send(JSON.stringify(policies));";
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  std::string json;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(contents, javascript,
                                                     &json));
  std::unique_ptr<base::Value> value_ptr = base::JSONReader::Read(json);
  const base::ListValue* actual_policies = NULL;
  ASSERT_TRUE(value_ptr.get());
  ASSERT_TRUE(value_ptr->GetAsList(&actual_policies));

  // Verify that the cells contain the expected strings for all policies.
  ASSERT_EQ(expected_policies.size(), actual_policies->GetSize());
  for (size_t i = 0; i < expected_policies.size(); ++i) {
    const std::vector<std::string> expected_policy = expected_policies[i];
    const base::ListValue* actual_policy;
    ASSERT_TRUE(actual_policies->GetList(i, &actual_policy));
    ASSERT_EQ(expected_policy.size(), actual_policy->GetSize());
    for (size_t j = 0; j < expected_policy.size(); ++j) {
      std::string value;
      ASSERT_TRUE(actual_policy->GetString(j, &value));
      EXPECT_EQ(expected_policy[j], value);
    }
  }
}

void PolicyUITest::VerifyExportingPolicies(
    const base::DictionaryValue& expected) {
  // Set SelectFileDialog to use our factory.
  ui::SelectFileDialog::SetFactory(new TestSelectFileDialogFactory());

  // Navigate to the about:policy page.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy"));

  // Click on 'save policies' button.
  const std::string javascript =
      "document.getElementById(\"export-policies\").click()";

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(contents, javascript));

  base::TaskScheduler::GetInstance()->FlushForTesting();
  // Open the created file.
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  std::string file_contents;
  EXPECT_TRUE(
      base::ReadFileToString(export_policies_test_file_path, &file_contents));

  std::unique_ptr<base::Value> value_ptr =
      base::JSONReader::Read(file_contents);

  // Check that the file contains a valid dictionary.
  EXPECT_TRUE(value_ptr.get());
  base::DictionaryValue* actual_policies = nullptr;
  EXPECT_TRUE(value_ptr->GetAsDictionary(&actual_policies));

  // Check that this dictionary is the same as expected.
  EXPECT_EQ(expected, *actual_policies);
}

IN_PROC_BROWSER_TEST_F(PolicyUITest, WritePoliciesToJSONFile) {
  // Set policy values and generate expected dictionary.
  policy::PolicyMap values;
  base::DictionaryValue expected_values;

  base::ListValue popups_blocked_for_urls;
  popups_blocked_for_urls.AppendString("aaa");
  popups_blocked_for_urls.AppendString("bbb");
  popups_blocked_for_urls.AppendString("ccc");
  values.Set(policy::key::kPopupsBlockedForUrls, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
             popups_blocked_for_urls.CreateDeepCopy(), nullptr);
  SetExpectedPolicy(&expected_values, policy::key::kPopupsBlockedForUrls,
                    "mandatory", "machine", "sourcePlatform",
                    popups_blocked_for_urls);

  values.Set(policy::key::kDefaultImagesSetting, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
             base::MakeUnique<base::Value>(2), nullptr);
  SetExpectedPolicy(&expected_values, policy::key::kDefaultImagesSetting,
                    "mandatory", "machine", "sourceCloud", base::Value(2));

  // This also checks that we save complex policies correctly.
  base::DictionaryValue unknown_policy;
  base::DictionaryValue body;
  body.SetInteger("first", 0);
  body.SetBoolean("second", true);
  unknown_policy.SetInteger("head", 12);
  unknown_policy.SetDictionary("body", body.CreateDeepCopy());
  const std::string kUnknownPolicy = "NoSuchThing";
  values.Set(kUnknownPolicy, policy::POLICY_LEVEL_RECOMMENDED,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             unknown_policy.CreateDeepCopy(), nullptr);
  SetExpectedPolicy(&expected_values, kUnknownPolicy, "recommended", "user",
                    "sourceCloud", unknown_policy);

  // Set the extension policies to an empty dictionary as we haven't added any
  // such policies.
  expected_values.SetDictionary("extensionPolicies",
                                base::MakeUnique<base::DictionaryValue>());

  UpdateProviderPolicy(values);

  // Check writing those policies to a newly created file.
  VerifyExportingPolicies(expected_values);

  // Change policy values.
  values.Erase(policy::key::kDefaultImagesSetting);
  expected_values.RemovePath(
      std::string("chromePolicies.") +
          std::string(policy::key::kDefaultImagesSetting),
      nullptr);

  // This also checks that we bypass the policy that blocks file selection
  // dialogs.
  values.Set(policy::key::kAllowFileSelectionDialogs,
             policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
             policy::POLICY_SOURCE_PLATFORM,
             base::MakeUnique<base::Value>(false), nullptr);
  SetExpectedPolicy(&expected_values, policy::key::kAllowFileSelectionDialogs,
                    "mandatory", "machine", "sourcePlatform",
                    base::Value(false));

  popups_blocked_for_urls.AppendString("ddd");
  values.Set(policy::key::kPopupsBlockedForUrls, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_PLATFORM,
             popups_blocked_for_urls.CreateDeepCopy(), nullptr);
  SetExpectedPolicy(&expected_values, policy::key::kPopupsBlockedForUrls,
                    "mandatory", "machine", "sourcePlatform",
                    popups_blocked_for_urls);

  UpdateProviderPolicy(values);

  // Check writing changed policies to the same file (should overwrite the
  // contents).
  VerifyExportingPolicies(expected_values);
}

IN_PROC_BROWSER_TEST_F(PolicyUITest, SendPolicyNames) {
  // Verifies that the names of known policies are sent to the UI and processed
  // there correctly by checking that the policy table contains all policies in
  // the correct order.

  // Expect that the policy table contains all known policies in alphabetical
  // order and none of the policies have a set value.
  std::vector<std::vector<std::string> > expected_policies;
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  ASSERT_TRUE(chrome_schema.valid());
  for (policy::Schema::Iterator it = chrome_schema.GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    expected_policies.push_back(
        PopulateExpectedPolicy(
            it.key(), std::string(), std::string(), nullptr, false));
  }

  // Retrieve the contents of the policy table from the UI and verify that it
  // matches the expectation.
  VerifyPolicies(expected_policies);
}

IN_PROC_BROWSER_TEST_F(PolicyUITest, SendPolicyValues) {
  // Verifies that policy values are sent to the UI and processed there
  // correctly by setting the values of four known and one unknown policy and
  // checking that the policy table contains the policy names, values and
  // metadata in the correct order.
  policy::PolicyMap values;
  std::map<std::string, std::string> expected_values;

  // Set the values of four existing policies.
  std::unique_ptr<base::ListValue> restore_on_startup_urls(new base::ListValue);
  restore_on_startup_urls->AppendString("aaa");
  restore_on_startup_urls->AppendString("bbb");
  restore_on_startup_urls->AppendString("ccc");
  values.Set(policy::key::kRestoreOnStartupURLs, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             std::move(restore_on_startup_urls), nullptr);
  expected_values[policy::key::kRestoreOnStartupURLs] = "aaa,bbb,ccc";
  values.Set(policy::key::kHomepageLocation, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
             base::MakeUnique<base::Value>("http://google.com"), nullptr);
  expected_values[policy::key::kHomepageLocation] = "http://google.com";
  values.Set(policy::key::kRestoreOnStartup, policy::POLICY_LEVEL_RECOMMENDED,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
             base::MakeUnique<base::Value>(4), nullptr);
  expected_values[policy::key::kRestoreOnStartup] = "4";
  values.Set(policy::key::kShowHomeButton, policy::POLICY_LEVEL_RECOMMENDED,
             policy::POLICY_SCOPE_MACHINE, policy::POLICY_SOURCE_CLOUD,
             base::MakeUnique<base::Value>(true), nullptr);
  expected_values[policy::key::kShowHomeButton] = "true";
  // Set the value of a policy that does not exist.
  const std::string kUnknownPolicy = "NoSuchThing";
  values.Set(kUnknownPolicy, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
             base::MakeUnique<base::Value>(true), nullptr);
  expected_values[kUnknownPolicy] = "true";
  const std::string kUnknownPolicyWithDots = "no.such.thing";
  values.Set(kUnknownPolicyWithDots, policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
             base::MakeUnique<base::Value>("blub"), nullptr);
  expected_values[kUnknownPolicyWithDots] = "blub";

  UpdateProviderPolicy(values);

  // Expect that the policy table contains, in order:
  // * All known policies whose value has been set, in alphabetical order.
  // * The unknown policy.
  // * All known policies whose value has not been set, in alphabetical order.
  std::vector<std::vector<std::string> > expected_policies;
  size_t first_unset_position = 0;
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  ASSERT_TRUE(chrome_schema.valid());
  for (policy::Schema::Iterator props = chrome_schema.GetPropertiesIterator();
       !props.IsAtEnd(); props.Advance()) {
    std::map<std::string, std::string>::const_iterator it =
        expected_values.find(props.key());
    const std::string value =
        it == expected_values.end() ? std::string() : it->second;
    const std::string source =
        it == expected_values.end() ? std::string() : "Cloud";
    const policy::PolicyMap::Entry* metadata = values.Get(props.key());
    expected_policies.insert(
        metadata ? expected_policies.begin() + first_unset_position++ :
                   expected_policies.end(),
        PopulateExpectedPolicy(props.key(), value, source, metadata, false));
  }
  expected_policies.insert(
      expected_policies.begin() + first_unset_position++,
      PopulateExpectedPolicy(kUnknownPolicy,
                             expected_values[kUnknownPolicy],
                             "Platform",
                             values.Get(kUnknownPolicy),
                             true));
  expected_policies.insert(
      expected_policies.begin() + first_unset_position++,
      PopulateExpectedPolicy(
          kUnknownPolicyWithDots, expected_values[kUnknownPolicyWithDots],
          "Platform", values.Get(kUnknownPolicyWithDots), true));

  // Retrieve the contents of the policy table from the UI and verify that it
  // matches the expectation.
  VerifyPolicies(expected_policies);
}

IN_PROC_BROWSER_TEST_F(PolicyUITest, ExtensionLoadAndSendPolicy) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIPolicyURL));
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  const std::string newly_added_policy_name = "new_policy";
  std::string json_data = "{\"type\": \"object\",\"properties\": {\"" +
                          newly_added_policy_name +
                          "\": { \"type\": \"string\"}}}";

  const std::string schema_file = "schema.json";
  base::FilePath schema_path = temp_dir_.GetPath().AppendASCII(schema_file);
  base::WriteFile(schema_path, json_data.data(), json_data.size());

  // Build extension that contains the policy schema.
  extensions::DictionaryBuilder storage;
  storage.Set("managed_schema", schema_file);

  extensions::DictionaryBuilder manifest;
  manifest.Set("name", "test")
      .Set("version", "1")
      .Set("manifest_version", 2)
      .Set("storage", storage.Build());

  extensions::ExtensionBuilder builder;
  builder.SetPath(temp_dir_.GetPath());
  builder.SetManifest(manifest.Build());

  // Install extension.
  ExtensionService* service = extensions::ExtensionSystem::Get(
                                  browser()->profile())->extension_service();
  EXPECT_CALL(provider_, RefreshPolicies());
  service->OnExtensionInstalled(builder.Build().get(), syncer::StringOrdinal(),
                                0);

  std::vector<std::vector<std::string>> expected_policies;
  policy::Schema chrome_schema =
      policy::Schema::Wrap(policy::GetChromeSchemaData());
  ASSERT_TRUE(chrome_schema.valid());

  for (policy::Schema::Iterator it = chrome_schema.GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    expected_policies.push_back(
        PopulateExpectedPolicy(
            it.key(), std::string(), std::string(), nullptr, false));
  }
  // Add newly added policy to expected policy list.
  expected_policies.push_back(PopulateExpectedPolicy(
      newly_added_policy_name, std::string(), std::string(), nullptr, false));

  // Verify if policy UI includes policy that extension have.
  VerifyPolicies(expected_policies);
}
