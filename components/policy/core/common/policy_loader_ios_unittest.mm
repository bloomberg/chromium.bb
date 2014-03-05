// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <UIKit/UIKit.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_loader_ios.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Key in the NSUserDefaults that contains the managed app configuration.
NSString* const kConfigurationKey = @"com.apple.configuration.managed";

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness();
  virtual ~TestHarness();

  virtual void SetUp() OVERRIDE;

  virtual ConfigurationPolicyProvider* CreateProvider(
      SchemaRegistry* registry,
      scoped_refptr<base::SequencedTaskRunner> task_runner) OVERRIDE;

  virtual void InstallEmptyPolicy() OVERRIDE;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) OVERRIDE;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) OVERRIDE;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) OVERRIDE;
  virtual void InstallStringListPolicy(
      const std::string& policy_name,
      const base::ListValue* policy_value) OVERRIDE;
  virtual void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) OVERRIDE;

  static PolicyProviderTestHarness* Create();

 private:
  // Merges the policies in |policy| into the current policy dictionary
  // in NSUserDefaults, after making sure that the policy dictionary
  // exists.
  void AddPolicies(NSDictionary* policy);

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

TestHarness::TestHarness()
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE) {}

TestHarness::~TestHarness() {
  // Cleanup any policies left from the test.
  [[NSUserDefaults standardUserDefaults] removeObjectForKey:kConfigurationKey];
}

void TestHarness::SetUp() {
  // Make sure there is no pre-existing policy present.
  [[NSUserDefaults standardUserDefaults] removeObjectForKey:kConfigurationKey];
}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  scoped_ptr<AsyncPolicyLoader> loader(new PolicyLoaderIOS(task_runner));
  return new AsyncPolicyProvider(registry, loader.Pass());
}

void TestHarness::InstallEmptyPolicy() {
  AddPolicies(@{});
}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  NSString* value = base::SysUTF8ToNSString(policy_value);
  AddPolicies(@{
      key: value
  });
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  AddPolicies(@{
      key: [NSNumber numberWithInt:policy_value]
  });
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  AddPolicies(@{
      key: [NSNumber numberWithBool:policy_value]
  });
}

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const base::ListValue* policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  base::ScopedCFTypeRef<CFPropertyListRef> value(ValueToProperty(policy_value));
  AddPolicies(@{
      key: static_cast<NSArray*>(value.get())
  });
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  base::ScopedCFTypeRef<CFPropertyListRef> value(ValueToProperty(policy_value));
  AddPolicies(@{
      key: static_cast<NSDictionary*>(value.get())
  });
}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness();
}

void TestHarness::AddPolicies(NSDictionary* policy) {
  NSString* key = @"ChromePolicy";
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  base::scoped_nsobject<NSMutableDictionary> chromePolicy(
      [[NSMutableDictionary alloc] init]);

  NSDictionary* previous = [defaults dictionaryForKey:key];
  if (previous)
    [chromePolicy addEntriesFromDictionary:previous];

  [chromePolicy addEntriesFromDictionary:policy];

  NSDictionary* wrapper = @{
      key: chromePolicy
  };
  [[NSUserDefaults standardUserDefaults] setObject:wrapper
                                            forKey:kConfigurationKey];
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_CASE_P(
    PolicyProviderIOSTest,
    ConfigurationPolicyProviderTest,
    testing::Values(TestHarness::Create));

}  // namespace policy
