// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/policy/configuration_policy_reader.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/mock_configuration_policy_reader.h"
#include "chrome/browser/policy/policy_map.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::_;

namespace policy {

class ConfigurationPolicyReaderTest : public testing::Test {
 protected:
  ConfigurationPolicyReaderTest() {
    EXPECT_CALL(provider_, ProvideInternal(_))
        .WillRepeatedly(CopyPolicyMap(&policy_));
    EXPECT_CALL(provider_, IsInitializationComplete())
        .WillRepeatedly(Return(true));

    reader_.reset(new ConfigurationPolicyReader(&provider_));
    status_ok_ = ASCIIToUTF16("OK");
  }

  DictionaryValue* CreateDictionary(const char* policy_name,
                                    Value* policy_value) {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString(PolicyStatusInfo::kNameDictPath, ASCIIToUTF16(policy_name));
    dict->SetString(PolicyStatusInfo::kLevelDictPath,
                    PolicyStatusInfo::GetPolicyLevelString(
                        POLICY_LEVEL_MANDATORY));
    dict->SetString(PolicyStatusInfo::kScopeDictPath,
                    PolicyStatusInfo::GetPolicyScopeString(
                        POLICY_SCOPE_USER));
    dict->Set(PolicyStatusInfo::kValueDictPath, policy_value);
    dict->SetBoolean(PolicyStatusInfo::kSetDictPath, true);
    dict->SetString(PolicyStatusInfo::kStatusDictPath, status_ok_);

    return dict;
  }

  PolicyMap policy_;
  MockConfigurationPolicyProvider provider_;
  scoped_ptr<ConfigurationPolicyReader> reader_;
  string16 status_ok_;
};

TEST_F(ConfigurationPolicyReaderTest, GetDefault) {
  EXPECT_EQ(NULL, reader_->GetPolicyStatus(key::kHomepageLocation));
}

// Test for list-valued policy settings.
TEST_F(ConfigurationPolicyReaderTest, SetListValue) {
  ListValue mandatory_value;
  mandatory_value.Append(Value::CreateStringValue("test1"));
  mandatory_value.Append(Value::CreateStringValue("test2"));
  ListValue recommended_value;
  recommended_value.Append(Value::CreateStringValue("test3"));
  recommended_value.Append(Value::CreateStringValue("test4"));
  policy_.Set(key::kRestoreOnStartupURLs, POLICY_LEVEL_MANDATORY,
              POLICY_SCOPE_USER, mandatory_value.DeepCopy());
  policy_.Set(key::kDisabledSchemes, POLICY_LEVEL_RECOMMENDED,
              POLICY_SCOPE_USER, recommended_value.DeepCopy());
  reader_->OnUpdatePolicy(&provider_);

  scoped_ptr<DictionaryValue> dict(
      CreateDictionary(key::kRestoreOnStartupURLs,
                       mandatory_value.DeepCopy()));
  scoped_ptr<DictionaryValue> result(
      reader_->GetPolicyStatus(key::kRestoreOnStartupURLs));
  EXPECT_TRUE(dict->Equals(result.get()));

  dict.reset(CreateDictionary(key::kDisabledSchemes,
                              recommended_value.DeepCopy()));
  dict->SetString(
      "level",
      PolicyStatusInfo::GetPolicyLevelString(POLICY_LEVEL_RECOMMENDED));
  result.reset(reader_->GetPolicyStatus(key::kDisabledSchemes));
  EXPECT_TRUE(dict->Equals(result.get()));
}

// Test for string-valued policy settings.
TEST_F(ConfigurationPolicyReaderTest, SetStringValue) {
  StringValue mandatory_value("http://chromium.org");
  StringValue recommended_value("http://www.google.com/q={searchTerms}");
  policy_.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              mandatory_value.DeepCopy());
  policy_.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_RECOMMENDED,
              POLICY_SCOPE_USER, recommended_value.DeepCopy());
  reader_->OnUpdatePolicy(&provider_);
  scoped_ptr<DictionaryValue> dict(
      CreateDictionary(key::kHomepageLocation,
                       mandatory_value.DeepCopy()));
  scoped_ptr<DictionaryValue> result(
      reader_->GetPolicyStatus(key::kHomepageLocation));
  EXPECT_TRUE(dict->Equals(result.get()));

  dict.reset(CreateDictionary(key::kDefaultSearchProviderSearchURL,
                              recommended_value.DeepCopy()));
  dict->SetString(
      "level",
      PolicyStatusInfo::GetPolicyLevelString(POLICY_LEVEL_RECOMMENDED));
  result.reset(reader_->GetPolicyStatus(key::kDefaultSearchProviderSearchURL));
  EXPECT_TRUE(dict->Equals(result.get()));
}

// Test for boolean-valued policy settings.
TEST_F(ConfigurationPolicyReaderTest, SetBooleanValue) {
  policy_.Set(key::kShowHomeButton, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              Value::CreateBooleanValue(true));
  policy_.Set(key::kHomepageIsNewTabPage, POLICY_LEVEL_RECOMMENDED,
              POLICY_SCOPE_USER, Value::CreateBooleanValue(false));
  reader_->OnUpdatePolicy(&provider_);
  scoped_ptr<DictionaryValue> dict(
      CreateDictionary(key::kShowHomeButton, Value::CreateBooleanValue(true)));
  scoped_ptr<DictionaryValue> result(
      reader_->GetPolicyStatus(key::kShowHomeButton));
  EXPECT_TRUE(dict->Equals(result.get()));

  dict.reset(CreateDictionary(key::kHomepageIsNewTabPage,
                              Value::CreateBooleanValue(false)));
  dict->SetString(
      "level",
      PolicyStatusInfo::GetPolicyLevelString(POLICY_LEVEL_RECOMMENDED));
  result.reset(reader_->GetPolicyStatus(key::kHomepageIsNewTabPage));
  EXPECT_TRUE(dict->Equals(result.get()));
}

// Test for integer-valued policy settings.
TEST_F(ConfigurationPolicyReaderTest, SetIntegerValue) {
  policy_.Set(key::kRestoreOnStartup, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
              Value::CreateIntegerValue(4));
  policy_.Set(key::kIncognitoModeAvailability, POLICY_LEVEL_RECOMMENDED,
              POLICY_SCOPE_USER, Value::CreateIntegerValue(2));
  reader_->OnUpdatePolicy(&provider_);
  scoped_ptr<DictionaryValue> dict(
      CreateDictionary(key::kRestoreOnStartup, Value::CreateIntegerValue(4)));
  scoped_ptr<DictionaryValue> result(
      reader_->GetPolicyStatus(key::kRestoreOnStartup));
  EXPECT_TRUE(dict->Equals(result.get()));

  dict.reset(CreateDictionary(key::kIncognitoModeAvailability,
                              Value::CreateIntegerValue(2)));
  dict->SetString(
      "level",
      PolicyStatusInfo::GetPolicyLevelString(POLICY_LEVEL_RECOMMENDED));
  result.reset(reader_->GetPolicyStatus(key::kIncognitoModeAvailability));
  EXPECT_TRUE(dict->Equals(result.get()));
}

class PolicyStatusTest : public testing::Test {
 protected:
  PolicyStatusTest() {
    managed_platform_ = new MockConfigurationPolicyReader();
    managed_cloud_ = new MockConfigurationPolicyReader();
    recommended_platform_ = new MockConfigurationPolicyReader();
    recommended_cloud_ = new MockConfigurationPolicyReader();

    policy_status_.reset(new PolicyStatus(managed_platform_,
                                          managed_cloud_,
                                          recommended_platform_,
                                          recommended_cloud_));
    status_ok_ = ASCIIToUTF16("OK");
    status_not_set_ = ASCIIToUTF16("Not set.");

    policy_list_ = GetChromePolicyDefinitionList();
    policy_list_size_ =
        static_cast<size_t>(policy_list_->end - policy_list_->begin);
  }

  void DontSendPolicies() {
    EXPECT_CALL(*managed_platform_, GetPolicyStatus(_))
        .WillRepeatedly(ReturnNull());
    EXPECT_CALL(*managed_cloud_, GetPolicyStatus(_))
        .WillRepeatedly(ReturnNull());
    EXPECT_CALL(*recommended_platform_, GetPolicyStatus(_))
        .WillRepeatedly(ReturnNull());
    EXPECT_CALL(*recommended_cloud_, GetPolicyStatus(_))
        .WillRepeatedly(ReturnNull());
  }

  void SendPolicies() {
    EXPECT_CALL(*managed_platform_, GetPolicyStatus(_))
        .WillRepeatedly(ReturnNull());
    EXPECT_CALL(*managed_cloud_, GetPolicyStatus(_))
        .WillRepeatedly(ReturnNull());
    EXPECT_CALL(*recommended_platform_, GetPolicyStatus(_))
        .WillRepeatedly(ReturnNull());
    EXPECT_CALL(*recommended_cloud_, GetPolicyStatus(_))
        .WillRepeatedly(ReturnNull());

    EXPECT_CALL(*managed_platform_, GetPolicyStatus(key::kInstantEnabled))
        .WillRepeatedly(Return(CreateDictionary(key::kInstantEnabled,
                                                POLICY_LEVEL_MANDATORY)));
    EXPECT_CALL(*managed_cloud_, GetPolicyStatus(key::kDisablePluginFinder))
        .WillRepeatedly(Return(CreateDictionary(key::kDisablePluginFinder,
                                                POLICY_LEVEL_MANDATORY)));
    EXPECT_CALL(*recommended_platform_, GetPolicyStatus(key::kSyncDisabled))
        .WillRepeatedly(Return(CreateDictionary(key::kSyncDisabled,
                                                POLICY_LEVEL_RECOMMENDED)));
    EXPECT_CALL(*recommended_cloud_, GetPolicyStatus(key::kTranslateEnabled))
        .WillRepeatedly(Return(CreateDictionary(key::kTranslateEnabled,
                                                POLICY_LEVEL_RECOMMENDED)));
  }

  DictionaryValue* CreateDictionary(const char* name, PolicyLevel level) {
    DictionaryValue* value = new DictionaryValue();
    value->SetString(PolicyStatusInfo::kNameDictPath, ASCIIToUTF16(name));
    value->SetString(PolicyStatusInfo::kLevelDictPath,
                     PolicyStatusInfo::GetPolicyLevelString(level));
    value->SetString(PolicyStatusInfo::kScopeDictPath,
                     PolicyStatusInfo::GetPolicyScopeString(POLICY_SCOPE_USER));
    value->SetBoolean(PolicyStatusInfo::kValueDictPath, true);
    value->SetBoolean(PolicyStatusInfo::kSetDictPath, true);
    value->SetString(PolicyStatusInfo::kStatusDictPath, status_ok_);

    return value;
  }

  void SetDictionaryPaths(DictionaryValue* dict,
                          const char* policy_name,
                          bool defined,
                          PolicyLevel level) {
    dict->SetString(PolicyStatusInfo::kNameDictPath,
                    ASCIIToUTF16(policy_name));
    if (defined) {
      dict->SetString(PolicyStatusInfo::kLevelDictPath,
                      PolicyStatusInfo::GetPolicyLevelString(level));
    }
  }

  MockConfigurationPolicyReader* managed_platform_;
  MockConfigurationPolicyReader* managed_cloud_;
  MockConfigurationPolicyReader* recommended_platform_;
  MockConfigurationPolicyReader* recommended_cloud_;
  scoped_ptr<PolicyStatus> policy_status_;
  const PolicyDefinitionList* policy_list_;
  size_t policy_list_size_;
  string16 status_ok_;
  string16 status_not_set_;
};

TEST_F(PolicyStatusTest, GetPolicyStatusListNoSetPolicies) {
  DontSendPolicies();
  bool any_policies_sent;
  scoped_ptr<ListValue> status_list(
      policy_status_->GetPolicyStatusList(&any_policies_sent));
  EXPECT_FALSE(any_policies_sent);
  EXPECT_EQ(policy_list_size_, status_list->GetSize());
}

TEST_F(PolicyStatusTest, GetPolicyStatusListSetPolicies) {
  SendPolicies();
  bool any_policies_sent;
  scoped_ptr<ListValue> status_list(
      policy_status_->GetPolicyStatusList(&any_policies_sent));
  EXPECT_TRUE(any_policies_sent);
  EXPECT_EQ(policy_list_size_, status_list->GetSize());

  scoped_ptr<DictionaryValue> undefined_dict(new DictionaryValue());
  undefined_dict->SetString(PolicyStatusInfo::kLevelDictPath, "");
  undefined_dict->SetString(PolicyStatusInfo::kScopeDictPath, "");
  undefined_dict->SetString(PolicyStatusInfo::kValueDictPath, "");
  undefined_dict->SetBoolean(PolicyStatusInfo::kSetDictPath, false);
  undefined_dict->SetString(PolicyStatusInfo::kStatusDictPath, status_not_set_);

  scoped_ptr<DictionaryValue> defined_dict(new DictionaryValue());
  defined_dict->SetString(PolicyStatusInfo::kScopeDictPath,
                          PolicyStatusInfo::GetPolicyScopeString(
                              POLICY_SCOPE_USER));
  defined_dict->Set(PolicyStatusInfo::kValueDictPath,
                    Value::CreateBooleanValue(true));
  defined_dict->SetBoolean(PolicyStatusInfo::kSetDictPath, true);
  defined_dict->SetString(PolicyStatusInfo::kStatusDictPath, status_ok_);

  struct {
    const char *name;
    PolicyLevel level;
  } cases[] = {
    { key::kInstantEnabled,       POLICY_LEVEL_MANDATORY },
    { key::kDisablePluginFinder,  POLICY_LEVEL_MANDATORY },
    { key::kSyncDisabled,         POLICY_LEVEL_RECOMMENDED },
    { key::kTranslateEnabled,     POLICY_LEVEL_RECOMMENDED },
    { NULL,                       POLICY_LEVEL_MANDATORY },
  };

  for (const PolicyDefinitionList::Entry* entry = policy_list_->begin;
       entry != policy_list_->end; ++entry) {
    Value* status_dict = NULL;

    // Every policy in |policy_list_| has to appear in the returned ListValue.
    string16 name = ASCIIToUTF16(entry->name);
    for (ListValue::const_iterator status_entry = status_list->begin();
         status_entry != status_list->end();
         ++status_entry) {
      string16 value;
      ASSERT_TRUE((*status_entry)->IsType(Value::TYPE_DICTIONARY));
      DictionaryValue* dict = static_cast<DictionaryValue*>(*status_entry);
      ASSERT_TRUE(dict->GetString(PolicyStatusInfo::kNameDictPath,
                                  &value));
      if (value == name)
        status_dict = *status_entry;
    }

    ASSERT_FALSE(status_dict == NULL);

    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
      if (entry->name == cases[i].name || cases[i].name == NULL) {
        DictionaryValue* dict =
            cases[i].name ? defined_dict.get() : undefined_dict.get();
        SetDictionaryPaths(dict,
                           entry->name,
                           cases[i].name != NULL,
                           cases[i].level);
        EXPECT_TRUE(dict->Equals(status_dict));
        break;
      }
    }
  }
}

} // namespace policy
