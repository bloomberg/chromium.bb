// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CONFIGURATION_POLICY_PROVIDER_TEST_H_
#define COMPONENTS_POLICY_CORE_COMMON_CONFIGURATION_POLICY_PROVIDER_TEST_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class DictionaryValue;
class ListValue;
class SequencedTaskRunner;
class Value;
}

namespace policy {

class ConfigurationPolicyProvider;

namespace test_keys {

extern const char kKeyString[];
extern const char kKeyBoolean[];
extern const char kKeyInteger[];
extern const char kKeyStringList[];
extern const char kKeyDictionary[];

}  // namespace test_keys

class PolicyTestBase : public testing::Test {
 public:
  PolicyTestBase();
  virtual ~PolicyTestBase();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  bool RegisterSchema(const PolicyNamespace& ns,
                      const std::string& schema);

  SchemaRegistry schema_registry_;

  // Create an actual IO loop (needed by FilePathWatcher).
  base::MessageLoopForIO loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyTestBase);
};

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
  virtual ConfigurationPolicyProvider* CreateProvider(
      SchemaRegistry* registry,
      scoped_refptr<base::SequencedTaskRunner> task_runner) = 0;

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

  // Not every provider supports installing 3rd party policy. Those who do
  // should override this method; the default just makes the test fail.
  virtual void Install3rdPartyPolicy(const base::DictionaryValue* policies);

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
    : public PolicyTestBase,
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
  scoped_ptr<ConfigurationPolicyProvider> provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyProviderTest);
};

// An extension of ConfigurationPolicyProviderTest that also tests loading of
// 3rd party policy. Policy provider implementations that support loading of
// 3rd party policy should also instantiate these tests.
class Configuration3rdPartyPolicyProviderTest
    : public ConfigurationPolicyProviderTest {
 protected:
  Configuration3rdPartyPolicyProviderTest();
  virtual ~Configuration3rdPartyPolicyProviderTest();

 private:
  DISALLOW_COPY_AND_ASSIGN(Configuration3rdPartyPolicyProviderTest);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CONFIGURATION_POLICY_PROVIDER_TEST_H_
