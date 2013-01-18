// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreFoundation/CoreFoundation.h>

#include "base/basictypes.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/sys_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/policy/async_policy_provider.h"
#include "chrome/browser/policy/configuration_policy_provider_test.h"
#include "chrome/browser/policy/policy_bundle.h"
#include "chrome/browser/policy/policy_loader_mac.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/preferences_mock_mac.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::mac::ScopedCFTypeRef;

namespace policy {

namespace {

// Converts a base::Value to the equivalent CFPropertyListRef.
// The returned value is owned by the caller.
CFPropertyListRef CreatePropertyFromValue(const base::Value* value) {
  switch (value->GetType()) {
    case base::Value::TYPE_NULL:
      return kCFNull;

    case base::Value::TYPE_BOOLEAN: {
      bool bool_value;
      if (value->GetAsBoolean(&bool_value))
        return bool_value ? kCFBooleanTrue : kCFBooleanFalse;
      break;
    }

    case base::Value::TYPE_INTEGER: {
      int int_value;
      if (value->GetAsInteger(&int_value)) {
        return CFNumberCreate(
            kCFAllocatorDefault, kCFNumberIntType, &int_value);
      }
      break;
    }

    case base::Value::TYPE_DOUBLE: {
      double double_value;
      if (value->GetAsDouble(&double_value)) {
        return CFNumberCreate(
            kCFAllocatorDefault, kCFNumberDoubleType, &double_value);
      }
      break;
    }

    case base::Value::TYPE_STRING: {
      std::string string_value;
      if (value->GetAsString(&string_value))
        return base::SysUTF8ToCFStringRef(string_value);
      break;
    }

    case base::Value::TYPE_DICTIONARY: {
      const base::DictionaryValue* dict_value;
      if (value->GetAsDictionary(&dict_value)) {
        // |dict| is owned by the caller.
        CFMutableDictionaryRef dict =
            CFDictionaryCreateMutable(kCFAllocatorDefault,
                                      dict_value->size(),
                                      &kCFTypeDictionaryKeyCallBacks,
                                      &kCFTypeDictionaryValueCallBacks);
        for (base::DictionaryValue::Iterator iterator(*dict_value);
             iterator.HasNext(); iterator.Advance()) {
          // CFDictionaryAddValue() retains both |key| and |value|, so make sure
          // the references are balanced.
          ScopedCFTypeRef<CFStringRef> key(
              base::SysUTF8ToCFStringRef(iterator.key()));
          ScopedCFTypeRef<CFPropertyListRef> cf_value(
              CreatePropertyFromValue(&iterator.value()));
          if (cf_value)
            CFDictionaryAddValue(dict, key, cf_value);
        }
        return dict;
      }
      break;
    }

    case base::Value::TYPE_LIST: {
      const base::ListValue* list;
      if (value->GetAsList(&list)) {
        CFMutableArrayRef array =
            CFArrayCreateMutable(NULL, list->GetSize(), &kCFTypeArrayCallBacks);
        for (base::ListValue::const_iterator it(list->begin());
             it != list->end(); ++it) {
          // CFArrayAppendValue() retains |value|, so make sure the reference
          // created by CreatePropertyFromValue() is released.
          ScopedCFTypeRef<CFPropertyListRef> cf_value(
              CreatePropertyFromValue(*it));
          if (cf_value)
            CFArrayAppendValue(array, cf_value);
        }
        return array;
      }
      break;
    }

    case base::Value::TYPE_BINARY:
      // This type isn't converted (though it can be represented as CFData)
      // because there's no equivalent JSON type, and policy values can only
      // take valid JSON values.
      break;
  }

  return NULL;
}

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness();
  virtual ~TestHarness();

  virtual void SetUp() OVERRIDE;

  virtual ConfigurationPolicyProvider* CreateProvider(
      const PolicyDefinitionList* policy_definition_list) OVERRIDE;

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
  MockPreferences* prefs_;

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

TestHarness::TestHarness()
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER) {}

TestHarness::~TestHarness() {}

void TestHarness::SetUp() {}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    const PolicyDefinitionList* policy_definition_list) {
  prefs_ = new MockPreferences();
  scoped_ptr<AsyncPolicyLoader> loader(
      new PolicyLoaderMac(policy_definition_list, prefs_));
  return new AsyncPolicyProvider(loader.Pass());
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
                                          const base::ListValue* policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  ScopedCFTypeRef<CFPropertyListRef> array(
      CreatePropertyFromValue(policy_value));
  ASSERT_TRUE(array);
  prefs_->AddTestItem(name, array, true);
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  ScopedCFTypeRef<CFStringRef> name(base::SysUTF8ToCFStringRef(policy_name));
  ScopedCFTypeRef<CFPropertyListRef> dict(
      CreatePropertyFromValue(policy_value));
  ASSERT_TRUE(dict);
  prefs_->AddTestItem(name, dict, true);
}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness();
}

}  // namespace

// Instantiate abstract test case for basic policy reading tests.
INSTANTIATE_TEST_CASE_P(
    PolicyProviderMacTest,
    ConfigurationPolicyProviderTest,
    testing::Values(TestHarness::Create));

// TODO(joaodasilva): instantiate Configuration3rdPartyPolicyProviderTest too
// once the mac loader supports 3rd party policy. http://crbug.com/108995

// Special test cases for some mac preferences details.
class PolicyLoaderMacTest : public PolicyTestBase {
 protected:
  PolicyLoaderMacTest()
      : prefs_(new MockPreferences()),
        loader_(new PolicyLoaderMac(&test_policy_definitions::kList, prefs_)),
        provider_(scoped_ptr<AsyncPolicyLoader>(loader_)) {}
  virtual ~PolicyLoaderMacTest() {}

  virtual void SetUp() OVERRIDE {
    PolicyTestBase::SetUp();
    provider_.Init();
  }

  virtual void TearDown() OVERRIDE {
    provider_.Shutdown();
    PolicyTestBase::TearDown();
  }

  MockPreferences* prefs_;
  PolicyLoaderMac* loader_;
  AsyncPolicyProvider provider_;
};

TEST_F(PolicyLoaderMacTest, Invalid) {
  ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(test_policy_definitions::kKeyString));
  const char buffer[] = "binary \xde\xad\xbe\xef data";
  ScopedCFTypeRef<CFDataRef> invalid_data(
      CFDataCreate(kCFAllocatorDefault,
                   reinterpret_cast<const UInt8 *>(buffer),
                   arraysize(buffer)));
  ASSERT_TRUE(invalid_data);
  prefs_->AddTestItem(name, invalid_data.get(), true);
  prefs_->AddTestItem(name, invalid_data.get(), false);

  // Make the provider read the updated |prefs_|.
  provider_.RefreshPolicies();
  loop_.RunUntilIdle();
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_.policies().Equals(kEmptyBundle));
}

TEST_F(PolicyLoaderMacTest, TestNonForcedValue) {
  ScopedCFTypeRef<CFStringRef> name(
      base::SysUTF8ToCFStringRef(test_policy_definitions::kKeyString));
  ScopedCFTypeRef<CFPropertyListRef> test_value(
      base::SysUTF8ToCFStringRef("string value"));
  ASSERT_TRUE(test_value.get());
  prefs_->AddTestItem(name, test_value.get(), false);

  // Make the provider read the updated |prefs_|.
  provider_.RefreshPolicies();
  loop_.RunUntilIdle();
  PolicyBundle expected_bundle;
  expected_bundle.Get(POLICY_DOMAIN_CHROME, "")
      .Set(test_policy_definitions::kKeyString, POLICY_LEVEL_RECOMMENDED,
           POLICY_SCOPE_USER, base::Value::CreateStringValue("string value"));
  EXPECT_TRUE(provider_.policies().Equals(expected_bundle));
}

TEST_F(PolicyLoaderMacTest, TestConversions) {
  base::DictionaryValue root;

  // base::Value::TYPE_NULL
  root.Set("null", base::Value::CreateNullValue());

  // base::Value::TYPE_BOOLEAN
  root.SetBoolean("false", false);
  root.SetBoolean("true", true);

  // base::Value::TYPE_INTEGER
  root.SetInteger("int", 123);
  root.SetInteger("zero", 0);

  // base::Value::TYPE_DOUBLE
  root.SetDouble("double", 123.456);
  root.SetDouble("zerod", 0.0);

  // base::Value::TYPE_STRING
  root.SetString("string", "the fox jumps over something");
  root.SetString("empty", "");

  // base::Value::TYPE_LIST
  base::ListValue list;
  root.Set("emptyl", list.DeepCopy());
  for (base::DictionaryValue::Iterator it(root); it.HasNext(); it.Advance())
    list.Append(it.value().DeepCopy());
  EXPECT_EQ(root.size(), list.GetSize());
  list.Append(root.DeepCopy());
  root.Set("list", list.DeepCopy());

  // base::Value::TYPE_DICTIONARY
  base::DictionaryValue dict;
  root.Set("emptyd", dict.DeepCopy());
  // Very meta.
  root.Set("dict", root.DeepCopy());

  ScopedCFTypeRef<CFPropertyListRef> property(CreatePropertyFromValue(&root));
  ASSERT_TRUE(property);
  scoped_ptr<base::Value> value(
      PolicyLoaderMac::CreateValueFromProperty(property));
  ASSERT_TRUE(value.get());

  EXPECT_TRUE(root.Equals(value.get()));
}

}  // namespace policy
