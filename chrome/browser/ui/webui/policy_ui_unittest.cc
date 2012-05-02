// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/ui/webui/policy_ui.h"
#include "grit/generated_resources.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

TEST(PolicyUITest, ListPolicyValues) {
  base::StringValue kHomepage("http://google.com");
  base::FundamentalValue kTrue(true);
  base::FundamentalValue kMaxConnections(10);
  base::ListValue kDisabledSchemes;
  kDisabledSchemes.Append(base::Value::CreateStringValue("aaa"));
  kDisabledSchemes.Append(base::Value::CreateStringValue("bbb"));
  kDisabledSchemes.Append(base::Value::CreateStringValue("ccc"));

  PolicyMap policies;
  policies.Set(key::kDisabledSchemes, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, kDisabledSchemes.DeepCopy());
  policies.Set(key::kHomepageLocation, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_MACHINE, kHomepage.DeepCopy());
  policies.Set(key::kMaxConnectionsPerProxy, POLICY_LEVEL_RECOMMENDED,
               POLICY_SCOPE_USER, kMaxConnections.DeepCopy());
  policies.Set(key::kShowHomeButton, POLICY_LEVEL_RECOMMENDED,
               POLICY_SCOPE_MACHINE, kTrue.DeepCopy());

  bool any_set = false;
  scoped_ptr<base::ListValue> list(
      PolicyUIHandler::GetPolicyStatusList(policies, &any_set));
  ASSERT_TRUE(list.get());

  // The policies are in the order defined in GetChromePolicyDefinitionList(),
  // which is sorted by name. Defined policies come before undefined policies.
  const PolicyDefinitionList* policy_list = GetChromePolicyDefinitionList();
  size_t policy_list_size = policy_list->end - policy_list->begin;
  EXPECT_EQ(policy_list_size, list->GetSize());
  EXPECT_TRUE(any_set);

  // Constants to compare against.
  const string16 kMandatory =
      l10n_util::GetStringUTF16(IDS_POLICY_LEVEL_MANDATORY);
  const string16 kRecommended =
      l10n_util::GetStringUTF16(IDS_POLICY_LEVEL_RECOMMENDED);
  const string16 kUser =
      l10n_util::GetStringUTF16(IDS_POLICY_SCOPE_USER);
  const string16 kMachine =
      l10n_util::GetStringUTF16(IDS_POLICY_SCOPE_MACHINE);
  const string16 kOK = l10n_util::GetStringUTF16(IDS_OK);
  const string16 kNotSet = l10n_util::GetStringUTF16(IDS_POLICY_NOT_SET);

  string16 string;
  bool boolean;
  base::Value* value;
  base::DictionaryValue* dict = NULL;
  ASSERT_TRUE(list->GetDictionary(0, &dict));
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kName, &string));
  EXPECT_EQ(ASCIIToUTF16("DisabledSchemes"), string);
  EXPECT_TRUE(dict->GetBoolean(PolicyUIHandler::kSet, &boolean));
  EXPECT_TRUE(boolean);
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kLevel, &string));
  EXPECT_EQ(kMandatory, string);
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kScope, &string));
  EXPECT_EQ(kUser, string);
  EXPECT_TRUE(dict->Get(PolicyUIHandler::kValue, &value));
  EXPECT_TRUE(base::Value::Equals(&kDisabledSchemes, value));
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kStatus, &string));
  EXPECT_EQ(kOK, string);

  ASSERT_TRUE(list->GetDictionary(1, &dict));
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kName, &string));
  EXPECT_EQ(ASCIIToUTF16("HomepageLocation"), string);
  EXPECT_TRUE(dict->GetBoolean(PolicyUIHandler::kSet, &boolean));
  EXPECT_TRUE(boolean);
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kLevel, &string));
  EXPECT_EQ(kMandatory, string);
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kScope, &string));
  EXPECT_EQ(kMachine, string);
  EXPECT_TRUE(dict->Get(PolicyUIHandler::kValue, &value));
  EXPECT_TRUE(base::Value::Equals(&kHomepage, value));
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kStatus, &string));
  EXPECT_EQ(kOK, string);

  ASSERT_TRUE(list->GetDictionary(2, &dict));
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kName, &string));
  EXPECT_EQ(ASCIIToUTF16("MaxConnectionsPerProxy"), string);
  EXPECT_TRUE(dict->GetBoolean(PolicyUIHandler::kSet, &boolean));
  EXPECT_TRUE(boolean);
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kLevel, &string));
  EXPECT_EQ(kRecommended, string);
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kScope, &string));
  EXPECT_EQ(kUser, string);
  EXPECT_TRUE(dict->Get(PolicyUIHandler::kValue, &value));
  EXPECT_TRUE(base::Value::Equals(&kMaxConnections, value));
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kStatus, &string));
  EXPECT_EQ(kOK, string);

  ASSERT_TRUE(list->GetDictionary(3, &dict));
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kName, &string));
  EXPECT_EQ(ASCIIToUTF16("ShowHomeButton"), string);
  EXPECT_TRUE(dict->GetBoolean(PolicyUIHandler::kSet, &boolean));
  EXPECT_TRUE(boolean);
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kLevel, &string));
  EXPECT_EQ(kRecommended, string);
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kScope, &string));
  EXPECT_EQ(kMachine, string);
  EXPECT_TRUE(dict->Get(PolicyUIHandler::kValue, &value));
  EXPECT_TRUE(base::Value::Equals(&kTrue, value));
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kStatus, &string));
  EXPECT_EQ(kOK, string);

  // All the others should be unset.
  for (size_t i = 4; i < policy_list_size; ++i) {
    ASSERT_TRUE(list->GetDictionary(i, &dict));
    EXPECT_TRUE(dict->GetBoolean(PolicyUIHandler::kSet, &boolean));
    EXPECT_FALSE(boolean);
    EXPECT_TRUE(dict->GetString(PolicyUIHandler::kLevel, &string));
    EXPECT_TRUE(string.empty());
    EXPECT_TRUE(dict->GetString(PolicyUIHandler::kScope, &string));
    EXPECT_TRUE(string.empty());
    EXPECT_TRUE(dict->GetString(PolicyUIHandler::kValue, &string));
    EXPECT_TRUE(string.empty());
    EXPECT_TRUE(dict->GetString(PolicyUIHandler::kStatus, &string));
    EXPECT_EQ(kNotSet, string);
  }
}

TEST(PolicyUITest, UnknownPolicy) {
  PolicyMap policies;
  policies.Set("NoSuchThing", POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));

  bool any_set = false;
  scoped_ptr<base::ListValue> list(
      PolicyUIHandler::GetPolicyStatusList(policies, &any_set));
  ASSERT_TRUE(list.get());
  EXPECT_FALSE(any_set);

  ASSERT_GE(list->GetSize(), 1u);
  const string16 kUnknown = l10n_util::GetStringUTF16(IDS_POLICY_UNKNOWN);
  string16 string;
  bool boolean;
  base::DictionaryValue* dict;
  ASSERT_TRUE(list->GetDictionary(0, &dict));
  EXPECT_TRUE(dict->GetBoolean(PolicyUIHandler::kSet, &boolean));
  EXPECT_TRUE(boolean);
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kLevel, &string));
  EXPECT_TRUE(string.empty());
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kScope, &string));
  EXPECT_TRUE(string.empty());
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kValue, &string));
  EXPECT_TRUE(string.empty());
  EXPECT_TRUE(dict->GetString(PolicyUIHandler::kStatus, &string));
  EXPECT_EQ(kUnknown, string);
}

} // namespace policy
