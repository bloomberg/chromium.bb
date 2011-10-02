// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_storage_unittest.h"

#include "base/json/json_writer.h"
#include "base/file_util.h"
#include "base/values.h"

namespace {

// Gets the pretty-printed JSON for a value.
std::string GetJSON(const Value& value) {
  std::string json;
  base::JSONWriter::Write(&value, true, &json);
  return json;
}

// Pretty-prints a set of strings.
std::string ToString(const std::set<std::string>& strings) {
  std::string string("{");
  for (std::set<std::string>::const_iterator it = strings.begin();
      it != strings.end(); ++it) {
    if (it != strings.begin()) {
      string.append(", ");
    }
    string.append(*it);
  }
  string.append("}");
  return string;
}

}  // namespace

// Returns whether the result of a storage operation has the expected settings
// and changed keys.
testing::AssertionResult SettingsEq(
    const char* _1, const char* _2, const char* _3,
    DictionaryValue* expected_settings,
    std::set<std::string>* expected_changed_keys,
    ExtensionSettingsStorage::Result actual) {
  if (actual.HasError()) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJSON(*expected_settings) <<
        ", " << ToString(*expected_changed_keys) << "\n" <<
        ", actual has error: " << actual.GetError();
  }
  if (expected_settings == NULL && actual.GetSettings() != NULL) {
    return testing::AssertionFailure() <<
        "Expected NULL settings, actual: " << GetJSON(*actual.GetSettings());
  }
  if (expected_changed_keys == NULL && actual.GetChangedKeys() != NULL) {
    return testing::AssertionFailure() <<
        "Expected NULL changed keys, actual: " <<
        ToString(*actual.GetChangedKeys());
  }
  if (expected_settings != NULL && actual.GetSettings() == NULL) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJSON(*expected_settings) << ", actual NULL";
  }
  if (expected_changed_keys != NULL && actual.GetChangedKeys() == NULL) {
    return testing::AssertionFailure() <<
        "Expected: " << ToString(*expected_changed_keys) << ", actual NULL";
  }
  if (expected_settings != actual.GetSettings() &&
      !expected_settings->Equals(actual.GetSettings())) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJSON(*expected_settings) <<
        ", actual: " << GetJSON(*actual.GetSettings());
  }
  if (expected_changed_keys != actual.GetChangedKeys() &&
        *expected_changed_keys != *actual.GetChangedKeys()) {
    return testing::AssertionFailure() <<
        "Expected: " << ToString(*expected_changed_keys) <<
        ", actual: " << ToString(*actual.GetChangedKeys());
  }
  return testing::AssertionSuccess();
}

ExtensionSettingsStorageTest::ExtensionSettingsStorageTest()
    : key1_("foo"),
      key2_("bar"),
      key3_("baz"),
      empty_dict_(new DictionaryValue),
      dict1_(new DictionaryValue),
      dict12_(new DictionaryValue),
      dict123_(new DictionaryValue),
      ui_thread_(BrowserThread::UI, MessageLoop::current()),
      file_thread_(BrowserThread::FILE, MessageLoop::current()) {
  val1_.reset(Value::CreateStringValue(key1_ + "Value"));
  val2_.reset(Value::CreateStringValue(key2_ + "Value"));
  val3_.reset(Value::CreateStringValue(key3_ + "Value"));

  list1_.push_back(key1_);
  list2_.push_back(key2_);
  list3_.push_back(key3_);
  list12_.push_back(key1_);
  list12_.push_back(key2_);
  list13_.push_back(key1_);
  list13_.push_back(key3_);
  list123_.push_back(key1_);
  list123_.push_back(key2_);
  list123_.push_back(key3_);

  set1_.insert(list1_.begin(), list1_.end());
  set2_.insert(list2_.begin(), list2_.end());
  set3_.insert(list3_.begin(), list3_.end());
  set12_.insert(list12_.begin(), list12_.end());
  set13_.insert(list13_.begin(), list13_.end());
  set123_.insert(list123_.begin(), list123_.end());

  dict1_->Set(key1_, val1_->DeepCopy());
  dict12_->Set(key1_, val1_->DeepCopy());
  dict12_->Set(key2_, val2_->DeepCopy());
  dict123_->Set(key1_, val1_->DeepCopy());
  dict123_->Set(key2_, val2_->DeepCopy());
  dict123_->Set(key3_, val3_->DeepCopy());
}

ExtensionSettingsStorageTest::~ExtensionSettingsStorageTest() {}

void ExtensionSettingsStorageTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  storage_.reset((GetParam())(temp_dir_.path(), "fakeExtension"));
  ASSERT_TRUE(storage_.get());
}

void ExtensionSettingsStorageTest::TearDown() {
  storage_.reset();
}

TEST_P(ExtensionSettingsStorageTest, GetWhenEmpty) {
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(list123_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, GetWithSingleValue) {
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), &set1_, storage_->Set(key1_, *val1_));

  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), NULL, storage_->Get(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key2_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key3_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), NULL, storage_->Get(list123_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), NULL, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, GetWithMultipleValues) {
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), &set12_, storage_->Set(*dict12_));

  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), NULL, storage_->Get(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key3_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), NULL, storage_->Get(list123_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), NULL, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, RemoveWhenEmpty) {
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &empty_set_, storage_->Remove(key1_));

  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(list1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, RemoveWithSingleValue) {
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), &set1_, storage_->Set(*dict1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &set1_, storage_->Remove(key1_));

  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key2_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(list1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(list12_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, RemoveWithMultipleValues) {
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict123_.get(), &set123_, storage_->Set(*dict123_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &set3_, storage_->Remove(key3_));

  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), NULL, storage_->Get(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key3_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), NULL, storage_->Get(list1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), NULL, storage_->Get(list12_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), NULL, storage_->Get(list13_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), NULL, storage_->Get(list123_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), NULL, storage_->Get());

  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &set12_, storage_->Remove(list12_));

  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key3_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(list1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(list12_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(list13_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(list123_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, SetWhenOverwriting) {
  DictionaryValue key1_val2;
  key1_val2.Set(key1_, val2_->DeepCopy());

  EXPECT_PRED_FORMAT3(SettingsEq,
      &key1_val2, &set1_, storage_->Set(key1_, *val2_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), &set12_, storage_->Set(*dict12_));

  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), NULL, storage_->Get(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key3_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), NULL, storage_->Get(list1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), NULL, storage_->Get(list12_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), NULL, storage_->Get(list13_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), NULL, storage_->Get(list123_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), NULL, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, ClearWhenEmpty) {
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &empty_set_, storage_->Clear());

  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(list123_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, ClearWhenNotEmpty) {
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), &set12_, storage_->Set(*dict12_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &set12_, storage_->Clear());

  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(list123_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get());
}

// Dots should be allowed in key names; they shouldn't be interpreted as
// indexing into a dictionary.
TEST_P(ExtensionSettingsStorageTest, DotsInKeyNames) {
  std::string dot_key("foo.bar");
  StringValue dot_value("baz.qux");
  std::vector<std::string> dot_list;
  dot_list.push_back(dot_key);
  std::set<std::string> dot_set;
  dot_set.insert(dot_key);
  DictionaryValue dot_dict;
  dot_dict.SetWithoutPathExpansion(dot_key, dot_value.DeepCopy());

  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(dot_key));

  EXPECT_PRED_FORMAT3(SettingsEq,
      &dot_dict, &dot_set, storage_->Set(dot_key, dot_value));
  EXPECT_PRED_FORMAT3(SettingsEq,
      &dot_dict, &empty_set_, storage_->Set(dot_key, dot_value));
  EXPECT_PRED_FORMAT3(SettingsEq,
      &dot_dict, NULL, storage_->Get(dot_key));

  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &dot_set, storage_->Remove(dot_key));
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &empty_set_, storage_->Remove(dot_key));
  EXPECT_PRED_FORMAT3(SettingsEq,
      &dot_dict, &dot_set, storage_->Set(dot_dict));
  EXPECT_PRED_FORMAT3(SettingsEq,
      &dot_dict, NULL, storage_->Get(dot_list));
  EXPECT_PRED_FORMAT3(SettingsEq,
      &dot_dict, NULL, storage_->Get());

  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &dot_set, storage_->Remove(dot_list));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get(dot_key));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, DotsInKeyNamesWithDicts) {
  DictionaryValue outer_dict;
  DictionaryValue* inner_dict = new DictionaryValue();
  outer_dict.Set("foo", inner_dict);
  inner_dict->Set("bar", Value::CreateStringValue("baz"));
  std::set<std::string> changed_keys;
  changed_keys.insert("foo");

  EXPECT_PRED_FORMAT3(SettingsEq,
      &outer_dict, &changed_keys, storage_->Set(outer_dict));
  EXPECT_PRED_FORMAT3(SettingsEq,
      &outer_dict, NULL, storage_->Get("foo"));
  EXPECT_PRED_FORMAT3(SettingsEq,
      empty_dict_.get(), NULL, storage_->Get("foo.bar"));
}

TEST_P(ExtensionSettingsStorageTest, ComplexChangedKeysScenarios) {
  // Test:
  //   - Setting over missing/changed/same keys, combinations.
  //   - Removing over missing and present keys, combinations.
  //   - Clearing.
  std::vector<std::string> complex_list;
  std::set<std::string> complex_set;
  DictionaryValue complex_dict;

  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), &set1_, storage_->Set(key1_, *val1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), &empty_set_, storage_->Set(key1_, *val1_));

  complex_dict.Clear();
  complex_dict.Set(key1_, val2_->DeepCopy());
  EXPECT_PRED_FORMAT3(SettingsEq,
      &complex_dict, &set1_, storage_->Set(key1_, *val2_));

  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &set1_, storage_->Remove(key1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &empty_set_, storage_->Remove(key1_));

  EXPECT_PRED_FORMAT3(SettingsEq,
      dict1_.get(), &set1_, storage_->Set(key1_, *val1_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &set1_, storage_->Clear());
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &empty_set_, storage_->Clear());

  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), &set12_, storage_->Set(*dict12_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict12_.get(), &empty_set_, storage_->Set(*dict12_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      dict123_.get(), &set3_, storage_->Set(*dict123_));

  complex_dict.Clear();
  complex_dict.Set(key1_, val2_->DeepCopy());
  complex_dict.Set(key2_, val2_->DeepCopy());
  complex_dict.Set("asdf", val1_->DeepCopy());
  complex_dict.Set("qwerty", val3_->DeepCopy());
  complex_set.clear();
  complex_set.insert(key1_);
  complex_set.insert("asdf");
  complex_set.insert("qwerty");
  EXPECT_PRED_FORMAT3(SettingsEq,
      &complex_dict, &complex_set, storage_->Set(complex_dict));

  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &set12_, storage_->Remove(list12_));
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &empty_set_, storage_->Remove(list12_));

  complex_list.clear();
  complex_list.push_back(key1_);
  complex_list.push_back("asdf");
  complex_set.clear();
  complex_set.insert("asdf");
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &complex_set, storage_->Remove(complex_list));

  complex_set.clear();
  complex_set.insert(key3_);
  complex_set.insert("qwerty");
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &complex_set, storage_->Clear());
  EXPECT_PRED_FORMAT3(SettingsEq,
      NULL, &empty_set_, storage_->Clear());
}
