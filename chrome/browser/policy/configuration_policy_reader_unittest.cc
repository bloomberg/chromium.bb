// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::_;

namespace policy {

class ConfigurationPolicyReaderTest : public testing::Test {
 protected:
  ConfigurationPolicyReaderTest() : provider_() {
    managed_reader_.reset(
        new ConfigurationPolicyReader(&provider_, PolicyStatusInfo::MANDATORY));
    recommended_reader_.reset(new ConfigurationPolicyReader(&provider_,
                                  PolicyStatusInfo::RECOMMENDED));
    status_ok_ = ASCIIToUTF16("OK");
  }

  DictionaryValue* CreateDictionary(const char* policy_name,
                                    Value* policy_value) {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString(std::string(PolicyStatusInfo::kNameDictPath),
                    ASCIIToUTF16(policy_name));
    dict->SetString(std::string(PolicyStatusInfo::kLevelDictPath),
        PolicyStatusInfo::GetPolicyLevelString(PolicyStatusInfo::MANDATORY));
    dict->SetString(std::string(PolicyStatusInfo::kSourceTypeDictPath),
        PolicyStatusInfo::GetSourceTypeString(PolicyStatusInfo::USER));
    dict->Set(std::string(PolicyStatusInfo::kValueDictPath), policy_value);
    dict->SetBoolean(std::string(PolicyStatusInfo::kSetDictPath), true);
    dict->SetString(std::string(PolicyStatusInfo::kStatusDictPath), status_ok_);

    return dict;
  }

  MockConfigurationPolicyProvider provider_;
  scoped_ptr<ConfigurationPolicyReader> managed_reader_;
  scoped_ptr<ConfigurationPolicyReader> recommended_reader_;
  string16 status_ok_;
};

TEST_F(ConfigurationPolicyReaderTest, GetDefault) {
  EXPECT_EQ(NULL, managed_reader_->GetPolicyStatus(kPolicyHomepageLocation));
}

// Test for list-valued policy settings.
TEST_F(ConfigurationPolicyReaderTest, SetListValue) {
  ListValue* in_value = new ListValue();
  in_value->Append(Value::CreateStringValue("test1"));
  in_value->Append(Value::CreateStringValue("test2"));
  provider_.AddPolicy(kPolicyRestoreOnStartupURLs, in_value);
  managed_reader_->OnUpdatePolicy(&provider_);

  scoped_ptr<DictionaryValue>
      dict(CreateDictionary(key::kRestoreOnStartupURLs, in_value->DeepCopy()));
  scoped_ptr<DictionaryValue> result(
      managed_reader_->GetPolicyStatus(kPolicyRestoreOnStartupURLs));
  EXPECT_TRUE(dict->Equals(result.get()));

  recommended_reader_->OnUpdatePolicy(&provider_);
  dict->SetString("level",
      PolicyStatusInfo::GetPolicyLevelString(PolicyStatusInfo::RECOMMENDED));
  result.reset(
      recommended_reader_->GetPolicyStatus(kPolicyRestoreOnStartupURLs));
  EXPECT_TRUE(dict->Equals(result.get()));
}

// Test for string-valued policy settings.
TEST_F(ConfigurationPolicyReaderTest, SetStringValue) {
  provider_.AddPolicy(kPolicyHomepageLocation,
      Value::CreateStringValue("http://chromium.org"));
  managed_reader_->OnUpdatePolicy(&provider_);
  scoped_ptr<DictionaryValue> dict(CreateDictionary(key::kHomepageLocation,
      Value::CreateStringValue("http://chromium.org")));
  scoped_ptr<DictionaryValue> result(
      managed_reader_->GetPolicyStatus(kPolicyHomepageLocation));
  EXPECT_TRUE(dict->Equals(result.get()));

  recommended_reader_->OnUpdatePolicy(&provider_);
  dict->SetString("level",
      PolicyStatusInfo::GetPolicyLevelString(PolicyStatusInfo::RECOMMENDED));
  result.reset(
      recommended_reader_->GetPolicyStatus(kPolicyHomepageLocation));
  EXPECT_TRUE(dict->Equals(result.get()));
}

// Test for boolean-valued policy settings.
TEST_F(ConfigurationPolicyReaderTest, SetBooleanValue) {
  provider_.AddPolicy(kPolicyShowHomeButton, Value::CreateBooleanValue(true));
  managed_reader_->OnUpdatePolicy(&provider_);
  scoped_ptr<DictionaryValue> dict(CreateDictionary(key::kShowHomeButton,
      Value::CreateBooleanValue(true)));
  scoped_ptr<DictionaryValue> result(
      managed_reader_->GetPolicyStatus(kPolicyShowHomeButton));
  EXPECT_TRUE(dict->Equals(result.get()));

  recommended_reader_->OnUpdatePolicy(&provider_);
  dict->SetString("level",
      PolicyStatusInfo::GetPolicyLevelString(PolicyStatusInfo::RECOMMENDED));
  result.reset(recommended_reader_->GetPolicyStatus(kPolicyShowHomeButton));
  EXPECT_TRUE(dict->Equals(result.get()));

  provider_.AddPolicy(kPolicyShowHomeButton, Value::CreateBooleanValue(false));
  managed_reader_->OnUpdatePolicy(&provider_);
  dict->Set(
      PolicyStatusInfo::kValueDictPath, Value::CreateBooleanValue(false));
  dict->SetString("level",
      PolicyStatusInfo::GetPolicyLevelString(PolicyStatusInfo::MANDATORY));
  result.reset(managed_reader_->GetPolicyStatus(kPolicyShowHomeButton));
  EXPECT_TRUE(dict->Equals(result.get()));

  recommended_reader_->OnUpdatePolicy(&provider_);
  dict->SetString("level",
      PolicyStatusInfo::GetPolicyLevelString(PolicyStatusInfo::RECOMMENDED));
  result.reset(recommended_reader_->GetPolicyStatus(kPolicyShowHomeButton));
  EXPECT_TRUE(dict->Equals(result.get()));
}

// Test for integer-valued policy settings.
TEST_F(ConfigurationPolicyReaderTest, SetIntegerValue) {
  provider_.AddPolicy(kPolicyRestoreOnStartup, Value::CreateIntegerValue(3));
  managed_reader_->OnUpdatePolicy(&provider_);
  scoped_ptr<DictionaryValue> dict(CreateDictionary(key::kRestoreOnStartup,
      Value::CreateIntegerValue(3)));
  scoped_ptr<DictionaryValue> result(
      managed_reader_->GetPolicyStatus(kPolicyRestoreOnStartup));
  EXPECT_TRUE(dict->Equals(result.get()));

  recommended_reader_->OnUpdatePolicy(&provider_);
  dict->SetString("level",
      PolicyStatusInfo::GetPolicyLevelString(PolicyStatusInfo::RECOMMENDED));
  result.reset(recommended_reader_->GetPolicyStatus(kPolicyRestoreOnStartup));
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

    EXPECT_CALL(*managed_platform_, GetPolicyStatus(kPolicyInstantEnabled))
        .WillRepeatedly(Return(CreateDictionary(key::kInstantEnabled,
                                                PolicyStatusInfo::MANDATORY)));
    EXPECT_CALL(*managed_cloud_, GetPolicyStatus(kPolicyDisablePluginFinder))
        .WillRepeatedly(Return(CreateDictionary(key::kDisablePluginFinder,
                                                PolicyStatusInfo::MANDATORY)));
    EXPECT_CALL(*recommended_platform_, GetPolicyStatus(kPolicySyncDisabled))
        .WillRepeatedly(
            Return(CreateDictionary(key::kSyncDisabled,
                                    PolicyStatusInfo::RECOMMENDED)));
    EXPECT_CALL(*recommended_cloud_, GetPolicyStatus(kPolicyTranslateEnabled))
        .WillRepeatedly(
            Return(CreateDictionary(key::kTranslateEnabled,
                                    PolicyStatusInfo::RECOMMENDED)));
  }

  DictionaryValue* CreateDictionary(const char* name,
                                    PolicyStatusInfo::PolicyLevel level) {
    DictionaryValue* value = new DictionaryValue();
    value->SetString(std::string(PolicyStatusInfo::kNameDictPath),
                     ASCIIToUTF16(name));
    value->SetString(std::string(PolicyStatusInfo::kLevelDictPath),
                     PolicyStatusInfo::GetPolicyLevelString(level));
    value->SetString(std::string(PolicyStatusInfo::kSourceTypeDictPath),
        PolicyStatusInfo::GetSourceTypeString(PolicyStatusInfo::USER));
    value->SetBoolean(std::string(PolicyStatusInfo::kValueDictPath), true);
    value->SetBoolean(std::string(PolicyStatusInfo::kSetDictPath), true);
    value->SetString(std::string(PolicyStatusInfo::kStatusDictPath),
                     status_ok_);

    return value;
  }

  void SetDictionaryPaths(DictionaryValue* dict,
                          const char* policy_name,
                          bool defined,
                          PolicyStatusInfo::PolicyLevel level) {
    dict->SetString(PolicyStatusInfo::kNameDictPath,
                    ASCIIToUTF16(policy_name));
    if (defined) {
      dict->SetString(std::string(PolicyStatusInfo::kLevelDictPath),
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
  undefined_dict->SetString(std::string(PolicyStatusInfo::kLevelDictPath),
                            PolicyStatusInfo::GetPolicyLevelString(
                                PolicyStatusInfo::LEVEL_UNDEFINED));
  undefined_dict->SetString(std::string(PolicyStatusInfo::kSourceTypeDictPath),
                            PolicyStatusInfo::GetSourceTypeString(
                                PolicyStatusInfo::SOURCE_TYPE_UNDEFINED));
  undefined_dict->Set(std::string(PolicyStatusInfo::kValueDictPath),
                      Value::CreateNullValue());
  undefined_dict->SetBoolean(std::string(PolicyStatusInfo::kSetDictPath),
                             false);
  undefined_dict->SetString(std::string(PolicyStatusInfo::kStatusDictPath),
                            string16());

  scoped_ptr<DictionaryValue> defined_dict(new DictionaryValue());
  defined_dict->SetString(std::string(PolicyStatusInfo::kSourceTypeDictPath),
      PolicyStatusInfo::GetSourceTypeString(PolicyStatusInfo::USER));
  defined_dict->Set(std::string(PolicyStatusInfo::kValueDictPath),
                    Value::CreateBooleanValue(true));
  defined_dict->SetBoolean(std::string(PolicyStatusInfo::kSetDictPath), true);
  defined_dict->SetString(std::string(PolicyStatusInfo::kStatusDictPath),
                          status_ok_);

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
      ASSERT_TRUE(dict->GetString(std::string(PolicyStatusInfo::kNameDictPath),
                                  &value));

      if (value == name) {
        status_dict = *status_entry;
      }
    }

    ASSERT_FALSE(status_dict == NULL);

    switch (entry->policy_type) {
      case kPolicyInstantEnabled:
        SetDictionaryPaths(defined_dict.get(),
                           entry->name,
                           true,
                           PolicyStatusInfo::MANDATORY);
        EXPECT_TRUE(defined_dict->Equals(status_dict));
        break;
      case kPolicyDisablePluginFinder:
        SetDictionaryPaths(defined_dict.get(),
                           entry->name,
                           true,
                           PolicyStatusInfo::MANDATORY);
        EXPECT_TRUE(defined_dict->Equals(status_dict));
        break;
      case kPolicySyncDisabled:
        SetDictionaryPaths(defined_dict.get(),
                           entry->name,
                           true,
                           PolicyStatusInfo::RECOMMENDED);
        EXPECT_TRUE(defined_dict->Equals(status_dict));
        break;
      case kPolicyTranslateEnabled:
        SetDictionaryPaths(defined_dict.get(),
                           entry->name,
                           true,
                           PolicyStatusInfo::RECOMMENDED);
        EXPECT_TRUE(defined_dict->Equals(status_dict));
        break;
      default:
        SetDictionaryPaths(undefined_dict.get(),
                           entry->name,
                           false,
                           PolicyStatusInfo::LEVEL_UNDEFINED);
        EXPECT_TRUE(undefined_dict->Equals(status_dict));
        break;
    }
  }
}

} // namespace policy
