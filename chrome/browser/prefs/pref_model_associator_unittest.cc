// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_model_associator.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class AbstractPreferenceMergeTest : public testing::Test {
 protected:
  virtual void SetUp() {
    pref_service_ = profile_.GetPrefs();
  }

  void SetContentPattern(base::DictionaryValue* patterns_dict,
                         const std::string& expression,
                         const std::string& content_type,
                         int setting) {
    base::DictionaryValue* expression_dict;
    bool found =
        patterns_dict->GetDictionaryWithoutPathExpansion(expression,
                                                         &expression_dict);
    if (!found) {
      expression_dict = new base::DictionaryValue;
      patterns_dict->SetWithoutPathExpansion(expression, expression_dict);
    }
    expression_dict->SetWithoutPathExpansion(
        content_type, new base::FundamentalValue(setting));
  }

  void SetPrefToEmpty(const std::string& pref_name) {
    scoped_ptr<base::Value> empty_value;
    const PrefService::Preference* pref =
        pref_service_->FindPreference(pref_name.c_str());
    ASSERT_TRUE(pref);
    base::Value::Type type = pref->GetType();
    if (type == base::Value::TYPE_DICTIONARY)
      empty_value.reset(new base::DictionaryValue);
    else if (type == base::Value::TYPE_LIST)
      empty_value.reset(new base::ListValue);
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
    server_url_list_.Append(new base::StringValue(server_url0_));
    server_url_list_.Append(new base::StringValue(server_url1_));
  }

  std::string server_url0_;
  std::string server_url1_;
  std::string local_url0_;
  std::string local_url1_;
  base::ListValue server_url_list_;
};

TEST_F(ListPreferenceMergeTest, NotListOrDictionary) {
  pref_service_->SetString(prefs::kHomePage, local_url0_);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kHomePage);
  scoped_ptr<base::Value> server_value(new base::StringValue(server_url0_));
  scoped_ptr<base::Value> merged_value(
      PrefModelAssociator::MergePreference(pref->name(),
                                           *pref->GetValue(),
                                           *server_value));
  EXPECT_TRUE(merged_value->Equals(server_value.get()));
}

TEST_F(ListPreferenceMergeTest, LocalEmpty) {
  SetPrefToEmpty(prefs::kURLsToRestoreOnStartup);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<base::Value> merged_value(
      PrefModelAssociator::MergePreference(pref->name(),
                                           *pref->GetValue(),
                                           server_url_list_));
  EXPECT_TRUE(merged_value->Equals(&server_url_list_));
}

TEST_F(ListPreferenceMergeTest, ServerNull) {
  scoped_ptr<base::Value> null_value(base::Value::CreateNullValue());
  {
    ListPrefUpdate update(pref_service_, prefs::kURLsToRestoreOnStartup);
    base::ListValue* local_list_value = update.Get();
    local_list_value->Append(new base::StringValue(local_url0_));
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<base::Value> merged_value(
      PrefModelAssociator::MergePreference(pref->name(),
                                           *pref->GetValue(),
                                           *null_value));
  const base::ListValue* local_list_value =
        pref_service_->GetList(prefs::kURLsToRestoreOnStartup);
  EXPECT_TRUE(merged_value->Equals(local_list_value));
}

TEST_F(ListPreferenceMergeTest, ServerEmpty) {
  scoped_ptr<base::Value> empty_value(new base::ListValue);
  {
    ListPrefUpdate update(pref_service_, prefs::kURLsToRestoreOnStartup);
    base::ListValue* local_list_value = update.Get();
    local_list_value->Append(new base::StringValue(local_url0_));
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<base::Value> merged_value(
      PrefModelAssociator::MergePreference(pref->name(),
                                           *pref->GetValue(),
                                           *empty_value));
  const base::ListValue* local_list_value =
        pref_service_->GetList(prefs::kURLsToRestoreOnStartup);
  EXPECT_TRUE(merged_value->Equals(local_list_value));
}

TEST_F(ListPreferenceMergeTest, Merge) {
  {
    ListPrefUpdate update(pref_service_, prefs::kURLsToRestoreOnStartup);
    base::ListValue* local_list_value = update.Get();
    local_list_value->Append(new base::StringValue(local_url0_));
    local_list_value->Append(new base::StringValue(local_url1_));
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<base::Value> merged_value(
      PrefModelAssociator::MergePreference(pref->name(),
                                           *pref->GetValue(),
                                           server_url_list_));

  base::ListValue expected;
  expected.Append(new base::StringValue(server_url0_));
  expected.Append(new base::StringValue(server_url1_));
  expected.Append(new base::StringValue(local_url0_));
  expected.Append(new base::StringValue(local_url1_));
  EXPECT_TRUE(merged_value->Equals(&expected));
}

TEST_F(ListPreferenceMergeTest, Duplicates) {
  {
    ListPrefUpdate update(pref_service_, prefs::kURLsToRestoreOnStartup);
    base::ListValue* local_list_value = update.Get();
    local_list_value->Append(new base::StringValue(local_url0_));
    local_list_value->Append(new base::StringValue(server_url0_));
    local_list_value->Append(new base::StringValue(server_url1_));
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<base::Value> merged_value(
      PrefModelAssociator::MergePreference(pref->name(),
                                           *pref->GetValue(),
                                           server_url_list_));

  base::ListValue expected;
  expected.Append(new base::StringValue(server_url0_));
  expected.Append(new base::StringValue(server_url1_));
  expected.Append(new base::StringValue(local_url0_));
  EXPECT_TRUE(merged_value->Equals(&expected));
}

TEST_F(ListPreferenceMergeTest, Equals) {
  {
    ListPrefUpdate update(pref_service_, prefs::kURLsToRestoreOnStartup);
    base::ListValue* local_list_value = update.Get();
    local_list_value->Append(new base::StringValue(server_url0_));
    local_list_value->Append(new base::StringValue(server_url1_));
  }

  scoped_ptr<base::Value> original(server_url_list_.DeepCopy());
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kURLsToRestoreOnStartup);
  scoped_ptr<base::Value> merged_value(
      PrefModelAssociator::MergePreference(pref->name(),
                                           *pref->GetValue(),
                                           server_url_list_));
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
  base::DictionaryValue server_patterns_;
};

TEST_F(DictionaryPreferenceMergeTest, LocalEmpty) {
  SetPrefToEmpty(prefs::kContentSettingsPatternPairs);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kContentSettingsPatternPairs);
  scoped_ptr<base::Value> merged_value(
      PrefModelAssociator::MergePreference(pref->name(),
                                           *pref->GetValue(),
                                           server_patterns_));
  EXPECT_TRUE(merged_value->Equals(&server_patterns_));
}

TEST_F(DictionaryPreferenceMergeTest, ServerNull) {
  scoped_ptr<base::Value> null_value(base::Value::CreateNullValue());
  {
    DictionaryPrefUpdate update(pref_service_,
                                prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression2_, content_type0_, 1);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kContentSettingsPatternPairs);
  scoped_ptr<base::Value> merged_value(
      PrefModelAssociator::MergePreference(pref->name(),
                                           *pref->GetValue(),
                                           *null_value));
  const base::DictionaryValue* local_dict_value =
      pref_service_->GetDictionary(prefs::kContentSettingsPatternPairs);
  EXPECT_TRUE(merged_value->Equals(local_dict_value));
}

TEST_F(DictionaryPreferenceMergeTest, ServerEmpty) {
  scoped_ptr<base::Value> empty_value(new base::DictionaryValue);
  {
    DictionaryPrefUpdate update(pref_service_,
                                prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression2_, content_type0_, 1);
  }

  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kContentSettingsPatternPairs);
  scoped_ptr<base::Value> merged_value(
      PrefModelAssociator::MergePreference(pref->name(),
                                           *pref->GetValue(),
                                           *empty_value));
  const base::DictionaryValue* local_dict_value =
      pref_service_->GetDictionary(prefs::kContentSettingsPatternPairs);
  EXPECT_TRUE(merged_value->Equals(local_dict_value));
}

TEST_F(DictionaryPreferenceMergeTest, MergeNoConflicts) {
  {
    DictionaryPrefUpdate update(pref_service_,
                                prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression2_, content_type0_, 1);
  }

  scoped_ptr<base::Value> merged_value(PrefModelAssociator::MergePreference(
     prefs::kContentSettingsPatternPairs,
      *pref_service_->FindPreference(prefs::kContentSettingsPatternPairs)->
          GetValue(),
      server_patterns_));

  base::DictionaryValue expected;
  SetContentPattern(&expected, expression0_, content_type0_, 1);
  SetContentPattern(&expected, expression0_, content_type1_, 2);
  SetContentPattern(&expected, expression1_, content_type0_, 1);
  SetContentPattern(&expected, expression2_, content_type0_, 1);
  EXPECT_TRUE(merged_value->Equals(&expected));
}

TEST_F(DictionaryPreferenceMergeTest, MergeConflicts) {
  {
    DictionaryPrefUpdate update(pref_service_,
                                prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression0_, content_type0_, 2);
    SetContentPattern(local_dict_value, expression1_, content_type0_, 1);
    SetContentPattern(local_dict_value, expression1_, content_type1_, 1);
    SetContentPattern(local_dict_value, expression2_, content_type0_, 2);
  }

  scoped_ptr<base::Value> merged_value(PrefModelAssociator::MergePreference(
      prefs::kContentSettingsPatternPairs,
      *pref_service_->FindPreference(prefs::kContentSettingsPatternPairs)->
          GetValue(),
      server_patterns_));

  base::DictionaryValue expected;
  SetContentPattern(&expected, expression0_, content_type0_, 1);
  SetContentPattern(&expected, expression0_, content_type1_, 2);
  SetContentPattern(&expected, expression1_, content_type0_, 1);
  SetContentPattern(&expected, expression1_, content_type1_, 1);
  SetContentPattern(&expected, expression2_, content_type0_, 2);
  EXPECT_TRUE(merged_value->Equals(&expected));
}

TEST_F(DictionaryPreferenceMergeTest, Equal) {
  {
    DictionaryPrefUpdate update(pref_service_,
                                prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression0_, content_type0_, 1);
    SetContentPattern(local_dict_value, expression0_, content_type1_, 2);
    SetContentPattern(local_dict_value, expression1_, content_type0_, 1);
  }

  scoped_ptr<base::Value> merged_value(PrefModelAssociator::MergePreference(
      prefs::kContentSettingsPatternPairs,
      *pref_service_->
          FindPreference(prefs::kContentSettingsPatternPairs)->GetValue(),
      server_patterns_));
  EXPECT_TRUE(merged_value->Equals(&server_patterns_));
}

TEST_F(DictionaryPreferenceMergeTest, ConflictButServerWins) {
  {
    DictionaryPrefUpdate update(pref_service_,
                                prefs::kContentSettingsPatternPairs);
    base::DictionaryValue* local_dict_value = update.Get();
    SetContentPattern(local_dict_value, expression0_, content_type0_, 2);
    SetContentPattern(local_dict_value, expression0_, content_type1_, 2);
    SetContentPattern(local_dict_value, expression1_, content_type0_, 1);
  }

  scoped_ptr<base::Value> merged_value(PrefModelAssociator::MergePreference(
      prefs::kContentSettingsPatternPairs,
      *pref_service_->
          FindPreference(prefs::kContentSettingsPatternPairs)->GetValue(),
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
    server_url_list_.Append(new base::StringValue(url0_));
    SetContentPattern(&server_patterns_, expression0_, content_type0_, 1);
  }

  bool MergeListPreference(const char* pref) {
    {
      ListPrefUpdate update(pref_service_, pref);
      base::ListValue* local_list_value = update.Get();
      local_list_value->Append(new base::StringValue(url1_));
    }

    scoped_ptr<base::Value> merged_value(PrefModelAssociator::MergePreference(
        pref,
        *pref_service_->GetUserPrefValue(pref),
        server_url_list_));

    base::ListValue expected;
    expected.Append(new base::StringValue(url0_));
    expected.Append(new base::StringValue(url1_));
    return merged_value->Equals(&expected);
  }

  bool MergeDictionaryPreference(const char* pref) {
    {
      DictionaryPrefUpdate update(pref_service_, pref);
      base::DictionaryValue* local_dict_value = update.Get();
      SetContentPattern(local_dict_value, expression1_, content_type0_, 1);
    }

    scoped_ptr<base::Value> merged_value(PrefModelAssociator::MergePreference(
        pref,
        *pref_service_->GetUserPrefValue(pref),
        server_patterns_));

    base::DictionaryValue expected;
    SetContentPattern(&expected, expression0_, content_type0_, 1);
    SetContentPattern(&expected, expression1_, content_type0_, 1);
    return merged_value->Equals(&expected);
  }

  std::string url0_;
  std::string url1_;
  std::string expression0_;
  std::string expression1_;
  std::string content_type0_;
  base::ListValue server_url_list_;
  base::DictionaryValue server_patterns_;
};

TEST_F(IndividualPreferenceMergeTest, URLsToRestoreOnStartup) {
  EXPECT_TRUE(MergeListPreference(prefs::kURLsToRestoreOnStartup));
}
