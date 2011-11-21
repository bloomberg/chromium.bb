// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "base/basictypes.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/policy/asynchronous_policy_test_base.h"
#include "chrome/browser/policy/configuration_policy_provider_mac.h"
#include "chrome/browser/policy/configuration_policy_provider_test.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/preferences_mock_mac.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::mac::ScopedCFTypeRef;

namespace policy {

namespace {

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness();
  virtual ~TestHarness();

  virtual void SetUp() OVERRIDE;

  virtual AsynchronousPolicyProvider* CreateProvider(
      const PolicyDefinitionList* policy_definition_list) OVERRIDE;

  virtual void InstallEmptyPolicy() OVERRIDE;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) OVERRIDE;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) OVERRIDE;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) OVERRIDE;
  virtual void InstallStringListPolicy(const std::string& policy_name,
                                       const ListValue* policy_value) OVERRIDE;

  static PolicyProviderTestHarness* Create();

 private:
  MockPreferences* prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

TestHarness::TestHarness() {}

TestHarness::~TestHarness() {}

void TestHarness::SetUp() {}

AsynchronousPolicyProvider* TestHarness::CreateProvider(
    const PolicyDefinitionList* policy_definition_list) {
  prefs_ = new MockPreferences();
  return new ConfigurationPolicyProviderMac(policy_definition_list, prefs_);
}

void TestHarness::InstallEmptyPolicy() {}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  ScopedCFTypeRef<CFStringRef> value(base::SysUTF8ToCFStringRef(policy_value));
  prefs_->AddTestItem(name, value, true);
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  ScopedCFTypeRef<CFNumberRef> value(
      CFNumberCreate(NULL, kCFNumberIntType, &policy_value));
  prefs_->AddTestItem(name, value, true);
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  prefs_->AddTestItem(name,
                      policy_value ? kCFBooleanTrue : kCFBooleanFalse,
                      true);
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const ListValue* policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  ScopedCFTypeRef<CFMutableArrayRef> array(
      CFArrayCreateMutable(NULL, policy_value->GetSize(),
                           &kCFTypeArrayCallBacks));
  for (ListValue::const_iterator element(policy_value->begin());
       element != policy_value->end(); ++element) {
    std::string element_value;
    if (!(*element)->GetAsString(&element_value))
      continue;
    ScopedCFTypeRef<CFStringRef> value(
        base::SysUTF8ToCFStringRef(element_value));
    CFArrayAppendValue(array, value);
  }

  prefs_->AddTestItem(name, array, true);
}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness();
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_CASE_P(
    ConfigurationPolicyProviderMacTest,
    ConfigurationPolicyProviderTest,
    testing::Values(TestHarness::Create));

// Special test cases for some mac preferences details.
class ConfigurationPolicyProviderMacTest : public AsynchronousPolicyTestBase {
 protected:
  ConfigurationPolicyProviderMacTest()
      : prefs_(new MockPreferences()),
        provider_(&test_policy_definitions::kList, prefs_) {}
  virtual ~ConfigurationPolicyProviderMacTest() {}

  MockPreferences* prefs_;
  ConfigurationPolicyProviderMac provider_;
};

TEST_F(ConfigurationPolicyProviderMacTest, Invalid) {
  ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(test_policy_definitions::kKeyString));
  ScopedCFTypeRef<CFDataRef> invalid_data(CFDataCreate(NULL, NULL, 0));
  prefs_->AddTestItem(name, invalid_data.get(), true);

  // Create the provider and have it read |prefs_|.
  provider_.RefreshPolicies();
  loop_.RunAllPending();
  PolicyMap policy_map;
  EXPECT_TRUE(provider_.Provide(&policy_map));
  EXPECT_TRUE(policy_map.empty());
}

TEST_F(ConfigurationPolicyProviderMacTest, TestNonForcedValue) {
  ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(test_policy_definitions::kKeyString));
  ScopedCFTypeRef<CFPropertyListRef> test_value(
      base::SysUTF8ToCFStringRef("string value"));
  ASSERT_TRUE(test_value.get());
  prefs_->AddTestItem(name, test_value.get(), false);

  // Create the provider and have it read |prefs_|.
  provider_.RefreshPolicies();
  loop_.RunAllPending();
  PolicyMap policy_map;
  EXPECT_TRUE(provider_.Provide(&policy_map));
  EXPECT_TRUE(policy_map.empty());
}

}  // namespace policy
