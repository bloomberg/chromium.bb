// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_TEST_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_TEST_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/policy/asynchronous_policy_test_base.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace policy {

class AsynchronousPolicyProvider;
struct PolicyDefinitionList;

// A stripped-down policy definition list that contains entries for the
// different policy setting types supported.
namespace test_policy_definitions {

// String policy keys.
extern const char kKeyString[];
extern const char kKeyBoolean[];
extern const char kKeyInteger[];
extern const char kKeyStringList[];
extern const char kKeyDictionary[];

// Policy definition list that contains entries for the keys above.
extern const PolicyDefinitionList kList;

}  // namespace test_policy_definitions

// An interface for creating a test policy provider and creating a policy
// provider instance for testing. Used as the parameter to the abstract
// ConfigurationPolicyProviderTest below.
class PolicyProviderTestHarness {
 public:
  // |level| and |scope| are the level and scope of the policies returned by
  // the providers from CreateProvider().
  PolicyProviderTestHarness(PolicyLevel level, PolicyScope scope);
  virtual ~PolicyProviderTestHarness();

  // Actions to run at gtest SetUp() time.
  virtual void SetUp() = 0;

  // Create a new policy provider.
  virtual AsynchronousPolicyProvider* CreateProvider(
      const PolicyDefinitionList* policy_definition_list) = 0;

  // Returns the policy level and scope set by the policy provider.
  PolicyLevel policy_level() const;
  PolicyScope policy_scope() const;

  // Helpers to configure the environment the policy provider reads from.
  virtual void InstallEmptyPolicy() = 0;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) = 0;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) = 0;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) = 0;
  virtual void InstallStringListPolicy(const std::string& policy_name,
                                       const base::ListValue* policy_value) = 0;
  virtual void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) = 0;

 private:
  PolicyLevel level_;
  PolicyScope scope_;

  DISALLOW_COPY_AND_ASSIGN(PolicyProviderTestHarness);
};

// A factory method for creating a test harness.
typedef PolicyProviderTestHarness* (*CreatePolicyProviderTestHarness)();

// Abstract policy provider test. This is meant to be instantiated for each
// policy provider implementation, passing in a suitable harness factory
// function as the test parameter.
class ConfigurationPolicyProviderTest
    : public AsynchronousPolicyTestBase,
      public testing::WithParamInterface<CreatePolicyProviderTestHarness> {
 protected:
  ConfigurationPolicyProviderTest();
  virtual ~ConfigurationPolicyProviderTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Installs a valid policy and checks whether the provider returns the
  // |expected_value|.
  void CheckValue(const char* policy_name,
                  const base::Value& expected_value,
                  base::Closure install_value);

  scoped_ptr<PolicyProviderTestHarness> test_harness_;
  scoped_ptr<AsynchronousPolicyProvider> provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProviderTest);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_PROVIDER_TEST_H_
