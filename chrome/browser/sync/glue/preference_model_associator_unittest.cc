// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/sync/glue/preference_model_associator.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::PreferenceModelAssociator;

class AbstractPreferenceMergeTest : public testing::Test {
 protected:
  virtual void SetUp() {
    pref_service_ = profile_.GetPrefs();
  }

  void SetContentPattern(DictionaryValue* patterns_dict,
                         const std::string& expression,
                         const std::string& content_type,
                         int setting) {
    DictionaryValue* expression_dict;
    bool found =
        patterns_dict->GetDictionaryWithoutPathExpansion(expression,
                                                         &expression_dict);
    if (!found) {
      expression_dict = new DictionaryValue;
      patterns_dict->SetWithoutPathExpansion(expression, expression_dict);
    }
    expression_dict->SetWithoutPathExpansion(
        content_type,
        Value::CreateIntegerValue(setting));
  }

  void SetPrefToEmpty(const std::string& pref_name) {
    scoped_ptr<Value> empty_value;
    const PrefService::Preference* pref =
        pref_service_->FindPreference(pref_name.c_str());
    ASSERT_TRUE(pref);
    Value::ValueType type = pref->GetType();
    if (type == Value::TYPE_DICTIONARY)
      empty_value.reset(new DictionaryValue);
    else if (type == Value::TYPE_LIST)
      empty_value.reset(new ListValue);
    else
      FAIL();
    pref_service_->Set(pref_name.c_str(), *empty_value);
  }

  TestingProfile profile_;
  PrefService* pref_service_;
};

class ListPreferenceMergeTest : public AbstractPreferenceMergeTest {
 protected:
  ListPreferenceMergeTest() :
      server_url0_("http://example.com/server0"),
      server_url1_("http://example.com/server1"),
      local_url0_("http://example.com/local0"),
      local_url1_("http://example.com/local1") {}

  virtual void SetUp() {
    AbstractPreferenceMergeTest::SetUp();
    server_url_list_.Append(Value::CreateStringValue(server_url0_));
    server_url_list_.Append(Value::CreateStringValue(server_url1_));
  }

  std::string server_url0_;
  std::string server_url1_;
  std::string local_url0_;
  std::string local_url1_;
  ListValue server_url_list_;
};

TEST_F(ListPreferenceMergeTest, NotListOrDictionary) {
  pref_service_->SetString(prefs::kHomePage, local_url0_);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kHomePage);
  scoped_ptr<Value> server_value(Value::CreateStringValue(server_url0_));
  scoped_ptr<Value> merged_value(
      PreferenceModelAssociator::MergePreference(*pref, *server_value));
  EXPECT_TRUE(merged_value->Equals(server_value.get()));
}

TEST_F(ListPreferenceMergeTest, LocalEmpty) {
  SetPrefToEmpty(prefs::kURLsToRestoreOnStartup);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<Value> merged_value(
      PreferenceModelAssociator::MergePreference(*pref, server_url_list_));
  EXPECT_TRUE(merged_value->Equals(&server_url_list_));
}

TEST_F(ListPreferenceMergeTest, ServerNull) {
  scoped_ptr<Value> null_value(Value::CreateNullValue());
  {
    ListPrefUpdate update(pref_service_, prefs::kURLsToRestoreOnStartup);
    ListValue* local_list_value = update.Get();
    local_list_value->Append(Value::CreateStringValue(local_url0_));
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<Value> merged_value(
      PreferenceModelAssociator::MergePreference(*pref, *null_value));
  const ListValue* local_list_value =
        pref_service_->GetList(prefs::kURLsToRestoreOnStartup);
  EXPECT_TRUE(merged_value->Equals(local_list_value));
}

TEST_F(ListPreferenceMergeTest, ServerEmpty) {
  scoped_ptr<Value> empty_value(new ListValue);
  {
    ListPrefUpdate update(pref_service_, prefs::kURLsToRestoreOnStartup);
    ListValue* local_list_value = update.Get();
    local_list_value->Append(Value::CreateStringValue(local_url0_));
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<Value> merged_value(
      PreferenceModelAssociator::MergePreference(*pref, *empty_value));
  const ListValue* local_list_value =
        pref_service_->GetList(prefs::kURLsToRestoreOnStartup);
  EXPECT_TRUE(merged_value->Equals(local_list_value));
}

TEST_F(ListPreferenceMergeTest, Merge) {
  {
    ListPrefUpdate update(pref_service_, prefs::kURLsToRestoreOnStartup);
    ListValue* local_list_value = update.Get();
    local_list_value->Append(Value::CreateStringValue(local_url0_));
    local_list_value->Append(Value::CreateStringValue(local_url1_));
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<Value> merged_value(
      PreferenceModelAssociator::MergePreference(*pref, server_url_list_));

  ListValue expected;
  expected.Append(Value::CreateStringValue(server_url0_));
  expected.Append(Value::CreateStringValue(server_url1_));
  expected.Append(Value::CreateStringValue(local_url0_));
  expected.Append(Value::CreateStringValue(local_url1_));
  EXPECT_TRUE(merged_value->Equals(&expected));
}

TEST_F(ListPreferenceMergeTest, Duplicates) {
  {
    ListPrefUpdate update(pref_service_, prefs::kURLsToRestoreOnStartup);
    ListValue* local_list_value = update.Get();
    local_list_value->Append(Value::CreateStringValue(local_url0_));
    local_list_value->Append(Value::CreateStringValue(server_url0_));
    local_list_value->Append(Value::CreateStringValue(server_url1_));
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<Value> merged_value(
      PreferenceModelAssociator::MergePreference(*pref, server_url_list_));

  ListValue expected;
  expected.Append(Value::CreateStringValue(server_url0_));
  expected.Append(Value::CreateStringValue(server_url1_));
  expected.Append(Value::CreateStringValue(local_url0_));
  EXPECT_TRUE(merged_value->Equals(&expected));
}

TEST_F(ListPreferenceMergeTest, Equals) {
  {
    ListPrefUpdate update(pref_service_, prefs::kURLsToRestoreOnStartup);
    ListValue* local_list_value = update.Get();
    local_list_value->Append(Value::CreateStringValue(server_url0_));
    local_list_value->Append(Value::CreateStringValue(server_url1_));
  }

  scoped_ptr<Value> original(server_url_list_.DeepCopy());
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<Value> merged_value(
      PreferenceModelAssociator::MergePreference(*pref, server_url_list_));
  EXPECT_TRUE(merged_value->Equals(original.get()));
}

class DictionaryPreferenceMergeTest : public AbstractPreferenceMergeTest {
 protected:
  DictionaryPreferenceMergeTest() :
      expression0_("expression0"),
      expression1_("expression1"),
      expression2_("expression2"),
      content_type0_("content_type0"),
      content_type1_("content_type1") {}

  virtual void SetUp() {
    AbstractPreferenceMergeTest::SetUp();
    SetContentPattern(&server_patterns_, expression0_, content_type0_, 1);
    SetContentPattern(&server_patterns_, expression0_, content_type1_, 2);
    SetContentPattern(&server_patterns_, expression1_, content_type0_, 1);
  }

  std::string expression0_;
  std::string expression1_;
  std::string expression2_;
  std::string content_type0_;
  std::string content_type1_;
  DictionaryValue server_patterns_;
};

TEST_F(DictionaryPreferenceMergeTest, LocalEmpty) {
  SetPrefToEmpty(prefs::kContentSettingsPatterns);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kContentSettingsPatterns);
  scoped_ptr<Value> merged_value(
      PreferenceModelAssociator::MergePreference(*pref, server_patterns_));
  EXPECT_TRUE(merged_value->Equals(&server_patterns_));
}

TEST_F(DictionaryPreferenceMergeTest, ServerNull) {
  scoped_ptr<Value> null_value(Value::CreateNullValue());
  {
    DictionaryPrefUpdate update(pref_service_, prefs::kContentSettingsPatterns);
    DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression2_, content_type0_, 1);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kContentSettingsPatterns);
  scoped_ptr<Value> merged_value(
      PreferenceModelAssociator::MergePreference(*pref, *null_value));
  const DictionaryValue* local_dict_value =
      pref_service_->GetDictionary(prefs::kContentSettingsPatterns);
  EXPECT_TRUE(merged_value->Equals(local_dict_value));
}

TEST_F(DictionaryPreferenceMergeTest, ServerEmpty) {
  scoped_ptr<Value> empty_value(new DictionaryValue);
  {
    DictionaryPrefUpdate update(pref_service_, prefs::kContentSettingsPatterns);
    DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression2_, content_type0_, 1);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kContentSettingsPatterns);
  scoped_ptr<Value> merged_value(
      PreferenceModelAssociator::MergePreference(*pref, *empty_value));
  const DictionaryValue* local_dict_value =
      pref_service_->GetDictionary(prefs::kContentSettingsPatterns);
  EXPECT_TRUE(merged_value->Equals(local_dict_value));
}

TEST_F(DictionaryPreferenceMergeTest, MergeNoConflicts) {
  {
    DictionaryPrefUpdate update(pref_service_, prefs::kContentSettingsPatterns);
    DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression2_, content_type0_, 1);
  }

  scoped_ptr<Value> merged_value(PreferenceModelAssociator::MergePreference(
      *pref_service_->FindPreference(prefs::kContentSettingsPatterns),
      server_patterns_));

  DictionaryValue expected;
  SetContentPattern(&expected, expression0_, content_type0_, 1);
  SetContentPattern(&expected, expression0_, content_type1_, 2);
  SetContentPattern(&expected, expression1_, content_type0_, 1);
  SetContentPattern(&expected, expression2_, content_type0_, 1);
  EXPECT_TRUE(merged_value->Equals(&expected));
}

TEST_F(DictionaryPreferenceMergeTest, MergeConflicts) {
  {
    DictionaryPrefUpdate update(pref_service_, prefs::kContentSettingsPatterns);
    DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression0_, content_type0_, 2);
    SetContentPattern(local_dict_value, expression1_, content_type0_, 1);
    SetContentPattern(local_dict_value, expression1_, content_type1_, 1);
    SetContentPattern(local_dict_value, expression2_, content_type0_, 2);
  }

  scoped_ptr<Value> merged_value(PreferenceModelAssociator::MergePreference(
      *pref_service_->FindPreference(prefs::kContentSettingsPatterns),
      server_patterns_));

  DictionaryValue expected;
  SetContentPattern(&expected, expression0_, content_type0_, 1);
  SetContentPattern(&expected, expression0_, content_type1_, 2);
  SetContentPattern(&expected, expression1_, content_type0_, 1);
  SetContentPattern(&expected, expression1_, content_type1_, 1);
  SetContentPattern(&expected, expression2_, content_type0_, 2);
  EXPECT_TRUE(merged_value->Equals(&expected));
}

TEST_F(DictionaryPreferenceMergeTest, Equal) {
  {
    DictionaryPrefUpdate update(pref_service_, prefs::kContentSettingsPatterns);
    DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression0_, content_type0_, 1);
    SetContentPattern(local_dict_value, expression0_, content_type1_, 2);
    SetContentPattern(local_dict_value, expression1_, content_type0_, 1);
  }

  scoped_ptr<Value> merged_value(PreferenceModelAssociator::MergePreference(
      *pref_service_->FindPreference(prefs::kContentSettingsPatterns),
      server_patterns_));
  EXPECT_TRUE(merged_value->Equals(&server_patterns_));
}

TEST_F(DictionaryPreferenceMergeTest, ConflictButServerWins) {
  {
    DictionaryPrefUpdate update(pref_service_, prefs::kContentSettingsPatterns);
    DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression0_, content_type0_, 2);
    SetContentPattern(local_dict_value, expression0_, content_type1_, 2);
    SetContentPattern(local_dict_value, expression1_, content_type0_, 1);
  }

  scoped_ptr<Value> merged_value(PreferenceModelAssociator::MergePreference(
      *pref_service_->FindPreference(prefs::kContentSettingsPatterns),
      server_patterns_));
  EXPECT_TRUE(merged_value->Equals(&server_patterns_));
}

class IndividualPreferenceMergeTest : public AbstractPreferenceMergeTest {
 protected:
  IndividualPreferenceMergeTest() :
      url0_("http://example.com/server0"),
      url1_("http://example.com/server1"),
      expression0_("expression0"),
      expression1_("expression1"),
      content_type0_("content_type0") {}

  virtual void SetUp() {
    AbstractPreferenceMergeTest::SetUp();
    server_url_list_.Append(Value::CreateStringValue(url0_));
    SetContentPattern(&server_patterns_, expression0_, content_type0_, 1);
  }

  bool MergeListPreference(const char* pref) {
    {
      ListPrefUpdate update(pref_service_, pref);
      ListValue* local_list_value = update.Get();
      local_list_value->Append(Value::CreateStringValue(url1_));
    }

    scoped_ptr<Value> merged_value(PreferenceModelAssociator::MergePreference(
        *pref_service_->FindPreference(pref),
        server_url_list_));

    ListValue expected;
    expected.Append(Value::CreateStringValue(url0_));
    expected.Append(Value::CreateStringValue(url1_));
    return merged_value->Equals(&expected);
  }

  bool MergeDictionaryPreference(const char* pref) {
    {
      DictionaryPrefUpdate update(pref_service_, pref);
      DictionaryValue* local_dict_value = update.Get();
      SetContentPattern(local_dict_value, expression1_, content_type0_, 1);
    }

    scoped_ptr<Value> merged_value(PreferenceModelAssociator::MergePreference(
        *pref_service_->FindPreference(pref),
        server_patterns_));

    DictionaryValue expected;
    SetContentPattern(&expected, expression0_, content_type0_, 1);
    SetContentPattern(&expected, expression1_, content_type0_, 1);
    return merged_value->Equals(&expected);
  }

  std::string url0_;
  std::string url1_;
  std::string expression0_;
  std::string expression1_;
  std::string content_type0_;
  ListValue server_url_list_;
  DictionaryValue server_patterns_;
};

TEST_F(IndividualPreferenceMergeTest, URLsToRestoreOnStartup) {
  EXPECT_TRUE(MergeListPreference(prefs::kURLsToRestoreOnStartup));
}

TEST_F(IndividualPreferenceMergeTest, DesktopNotificationAllowedOrigins) {
  EXPECT_TRUE(MergeListPreference(prefs::kDesktopNotificationAllowedOrigins));
}

TEST_F(IndividualPreferenceMergeTest, DesktopNotificationDeniedOrigins) {
  EXPECT_TRUE(MergeListPreference(prefs::kDesktopNotificationDeniedOrigins));
}

TEST_F(IndividualPreferenceMergeTest, ContentSettingsPatterns) {
  EXPECT_TRUE(MergeDictionaryPreference(prefs::kContentSettingsPatterns));
}

TEST_F(IndividualPreferenceMergeTest, GeolocationContentSettings) {
  EXPECT_TRUE(MergeDictionaryPreference(prefs::kGeolocationContentSettings));
}
