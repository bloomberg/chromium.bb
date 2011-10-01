// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_storage_unittest.h"

#include "base/json/json_writer.h"
#include "base/file_util.h"
#include "base/values.h"

// Gets the pretty-printed JSON for a value.
static std::string GetJSON(const Value& value) {
  std::string json;
  base::JSONWriter::Write(&value, true, &json);
  return json;
}

// Returns whether the result of a storage operation is an expected value.
testing::AssertionResult SettingsEq(
    const char* expected_expr, const char* actual_expr,
    DictionaryValue* expected, ExtensionSettingsStorage::Result actual) {
  if (actual.HasError()) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJSON(*expected) <<
        ", actual has error: " << actual.GetError();
  }
  if (expected == actual.GetSettings()) {
    return testing::AssertionSuccess();
  }
  if (expected == NULL && actual.GetSettings() != NULL) {
    return testing::AssertionFailure() <<
        "Expected NULL, actual: " << GetJSON(*actual.GetSettings());
  }
  if (expected != NULL && actual.GetSettings() == NULL) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJSON(*expected) << ", actual NULL";
  }
  if (!expected->Equals(actual.GetSettings())) {
    return testing::AssertionFailure() <<
        "Expected: " << GetJSON(*expected) <<
        ", actual: " << GetJSON(*actual.GetSettings());
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
  list12_.push_back(key1_);
  list12_.push_back(key2_);
  list13_.push_back(key1_);
  list13_.push_back(key3_);
  list123_.push_back(key1_);
  list123_.push_back(key2_);
  list123_.push_back(key3_);

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
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key1_));
  ASSERT_PRED_FORMAT2(SettingsEq,
      empty_dict_.get(), storage_->Get(empty_list_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(list123_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, GetWithSingleValue) {
  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Set(key1_, *val1_));

  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Get(key1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key2_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key3_));
  ASSERT_PRED_FORMAT2(SettingsEq,
      empty_dict_.get(), storage_->Get(empty_list_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Get(list123_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, GetWithMultipleValues) {
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Set(*dict12_));

  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Get(key1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key3_));
  ASSERT_PRED_FORMAT2(SettingsEq,
      empty_dict_.get(), storage_->Get(empty_list_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Get(list123_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, RemoveWhenEmpty) {
  ASSERT_PRED_FORMAT2(SettingsEq, NULL, storage_->Remove(key1_));

  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(list1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, RemoveWithSingleValue) {
  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Set(*dict1_));
  ASSERT_PRED_FORMAT2(SettingsEq, NULL, storage_->Remove(key1_));

  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key2_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(list1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(list12_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, RemoveWithMultipleValues) {
  ASSERT_PRED_FORMAT2(SettingsEq, dict123_.get(), storage_->Set(*dict123_));
  ASSERT_PRED_FORMAT2(SettingsEq, NULL, storage_->Remove(key3_));

  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Get(key1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key3_));
  ASSERT_PRED_FORMAT2(SettingsEq,
      empty_dict_.get(), storage_->Get(empty_list_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Get(list1_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Get(list12_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Get(list13_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Get(list123_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Get());

  ASSERT_PRED_FORMAT2(SettingsEq, NULL, storage_->Remove(list12_));

  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key3_));
  ASSERT_PRED_FORMAT2(SettingsEq,
      empty_dict_.get(), storage_->Get(empty_list_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(list1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(list12_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(list13_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(list123_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, SetWhenOverwriting) {
  DictionaryValue key1_val2;
  key1_val2.Set(key1_, val2_->DeepCopy());
  ASSERT_PRED_FORMAT2(SettingsEq, &key1_val2, storage_->Set(key1_, *val2_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Set(*dict12_));

  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Get(key1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key3_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(),
      storage_->Get(empty_list_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Get(list1_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Get(list12_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict1_.get(), storage_->Get(list13_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Get(list123_));
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, ClearWhenEmpty) {
  ASSERT_PRED_FORMAT2(SettingsEq, NULL, storage_->Clear());

  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(),
      storage_->Get(empty_list_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(list123_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, ClearWhenNotEmpty) {
  ASSERT_PRED_FORMAT2(SettingsEq, dict12_.get(), storage_->Set(*dict12_));
  ASSERT_PRED_FORMAT2(SettingsEq, NULL, storage_->Clear());

  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(key1_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(),
      storage_->Get(empty_list_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(list123_));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get());
}

// Dots should be allowed in key names; they shouldn't be interpreted as
// indexing into a dictionary.
TEST_P(ExtensionSettingsStorageTest, DotsInKeyNames) {
  std::string dot_key("foo.bar");
  StringValue dot_value("baz.qux");
  std::vector<std::string> dot_list;
  dot_list.push_back(dot_key);
  DictionaryValue dot_dict;
  dot_dict.SetWithoutPathExpansion(dot_key, dot_value.DeepCopy());

  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(dot_key));

  ASSERT_PRED_FORMAT2(SettingsEq, &dot_dict, storage_->Set(dot_key, dot_value));
  ASSERT_PRED_FORMAT2(SettingsEq, &dot_dict, storage_->Get(dot_key));

  ASSERT_PRED_FORMAT2(SettingsEq, NULL, storage_->Remove(dot_key));
  ASSERT_PRED_FORMAT2(SettingsEq, &dot_dict, storage_->Set(dot_dict));
  ASSERT_PRED_FORMAT2(SettingsEq, &dot_dict, storage_->Get(dot_list));
  ASSERT_PRED_FORMAT2(SettingsEq, &dot_dict, storage_->Get());

  ASSERT_PRED_FORMAT2(SettingsEq, NULL, storage_->Remove(dot_list));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get(dot_key));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, DotsInKeyNamesWithDicts) {
  DictionaryValue outer_dict;
  DictionaryValue* inner_dict = new DictionaryValue();
  outer_dict.Set("foo", inner_dict);
  inner_dict->Set("bar", Value::CreateStringValue("baz"));

  ASSERT_PRED_FORMAT2(SettingsEq, &outer_dict, storage_->Set(outer_dict));
  ASSERT_PRED_FORMAT2(SettingsEq, &outer_dict, storage_->Get("foo"));
  ASSERT_PRED_FORMAT2(SettingsEq, empty_dict_.get(), storage_->Get("foo.bar"));
}
