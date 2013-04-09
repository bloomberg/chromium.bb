// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::AnyNumber;
using testing::Return;
using testing::_;

namespace {

std::vector<std::string> PopulateExpectedPolicy(
    const std::string& name,
    const std::string& value,
    const policy::PolicyMap::Entry* metadata,
    bool unknown) {
  std::vector<std::string> expected_policy;

  // Populate expected scope.
  if (metadata) {
    expected_policy.push_back(l10n_util::GetStringUTF8(
        metadata->scope == policy::POLICY_SCOPE_MACHINE ?
            IDS_POLICY_SCOPE_DEVICE : IDS_POLICY_SCOPE_USER));
  } else {
    expected_policy.push_back("");
  }

  // Populate expected level.
  if (metadata) {
    expected_policy.push_back(l10n_util::GetStringUTF8(
        metadata->level == policy::POLICY_LEVEL_RECOMMENDED ?
            IDS_POLICY_LEVEL_RECOMMENDED : IDS_POLICY_LEVEL_MANDATORY));
  } else {
    expected_policy.push_back("");
  }

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

}  // namespace

class PolicyUITest : public InProcessBrowserTest {
 public:
  PolicyUITest();
  virtual ~PolicyUITest();

 protected:
  // InProcessBrowserTest implementation.
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;

  void UpdateProviderPolicy(const policy::PolicyMap& policy);

  void VerifyPolicies(const std::vector<std::vector<std::string> > expected);

 private:
  policy::MockConfigurationPolicyProvider provider_;

  DISALLOW_COPY_AND_ASSIGN(PolicyUITest);
};

PolicyUITest::PolicyUITest() {
}

PolicyUITest::~PolicyUITest() {
}

void PolicyUITest::SetUpInProcessBrowserTestFixture() {
  EXPECT_CALL(provider_, IsInitializationComplete(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(provider_, RegisterPolicyDomain(_, _)).Times(AnyNumber());
  policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
}

void PolicyUITest::UpdateProviderPolicy(const policy::PolicyMap& policy) {
 provider_.UpdateChromePolicy(policy);
 base::RunLoop loop;
 loop.RunUntilIdle();
}

void PolicyUITest::VerifyPolicies(
    const std::vector<std::vector<std::string> > expected_policies) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://policy"));

  // Retrieve the text contents of the policy table cells for all policies.
  const std::string javascript =
      "var entries = document.querySelectorAll("
      "    'table#policy-table > tbody');"
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
  scoped_ptr<base::Value> value_ptr(base::JSONReader::Read(json));
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

IN_PROC_BROWSER_TEST_F(PolicyUITest, SendPolicyNames) {
  // Verifies that the names of known policies are sent to the UI and processed
  // there correctly by checking that the policy table contains all policies in
  // the correct order.

  // Expect that the policy table contains all known policies in alphabetical
  // order and none of the policies have a set value.
  std::vector<std::vector<std::string> > expected_policies;
  const policy::PolicyDefinitionList* policies =
      policy::GetChromePolicyDefinitionList();
  for (const policy::PolicyDefinitionList::Entry* policy = policies->begin;
      policy != policies->end; ++policy) {
    expected_policies.push_back(
        PopulateExpectedPolicy(policy->name, "", NULL, false));
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
  base::ListValue* restore_on_startup_urls = new base::ListValue;
  restore_on_startup_urls->Append(base::Value::CreateStringValue("aaa"));
  restore_on_startup_urls->Append(base::Value::CreateStringValue("bbb"));
  restore_on_startup_urls->Append(base::Value::CreateStringValue("ccc"));
  values.Set(policy::key::kRestoreOnStartupURLs,
             policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             restore_on_startup_urls);
  expected_values[policy::key::kRestoreOnStartupURLs] = "aaa,bbb,ccc";
  values.Set(policy::key::kHomepageLocation,
             policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_MACHINE,
             base::Value::CreateStringValue("http://google.com"));
  expected_values[policy::key::kHomepageLocation] = "http://google.com";
  values.Set(policy::key::kRestoreOnStartup,
             policy::POLICY_LEVEL_RECOMMENDED,
             policy::POLICY_SCOPE_USER,
             base::Value::CreateIntegerValue(4));
  expected_values[policy::key::kRestoreOnStartup] = "4";
  values.Set(policy::key::kShowHomeButton,
             policy::POLICY_LEVEL_RECOMMENDED,
             policy::POLICY_SCOPE_MACHINE,
             base::Value::CreateBooleanValue(true));
  expected_values[policy::key::kShowHomeButton] = "true";
  // Set the value of a policy that does not exist.
  const std::string kUnknownPolicy = "NoSuchThing";
  values.Set(kUnknownPolicy,
             policy::POLICY_LEVEL_MANDATORY,
             policy::POLICY_SCOPE_USER,
             base::Value::CreateBooleanValue(true));
  expected_values[kUnknownPolicy] = "true";
  UpdateProviderPolicy(values);

  // Expect that the policy table contains, in order:
  // * All known policies whose value has been set, in alphabetical order.
  // * The unknown policy.
  // * All known policies whose value has not been set, in alphabetical order.
  std::vector<std::vector<std::string> > expected_policies;
  size_t first_unset_position = 0;
  const policy::PolicyDefinitionList* policies =
      policy::GetChromePolicyDefinitionList();
  for (const policy::PolicyDefinitionList::Entry* policy = policies->begin;
       policy != policies->end; ++policy) {
    std::map<std::string, std::string>::const_iterator it =
        expected_values.find(policy->name);
    const std::string value = it == expected_values.end() ? "" : it->second;
    const policy::PolicyMap::Entry* metadata = values.Get(policy->name);
    expected_policies.insert(
        metadata ? expected_policies.begin() + first_unset_position++ :
                   expected_policies.end(),
        PopulateExpectedPolicy(policy->name, value, metadata, false));
  }
  expected_policies.insert(
      expected_policies.begin() + first_unset_position++,
      PopulateExpectedPolicy(kUnknownPolicy,
                             expected_values[kUnknownPolicy],
                             values.Get(kUnknownPolicy),
                             true));

  // Retrieve the contents of the policy table from the UI and verify that it
  // matches the expectation.
  VerifyPolicies(expected_policies);
}
