// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/settings/settings_storage_unittest.h"

#include "base/json/json_writer.h"
#include "base/file_util.h"
#include "base/values.h"

namespace extensions {

using content::BrowserThread;

namespace {

// To save typing SettingsStorage::DEFAULTS everywhere.
const SettingsStorage::WriteOptions DEFAULTS = SettingsStorage::DEFAULTS;

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

// Compares two possibly NULL values for equality, filling |error| with an
// appropriate error message if they're different.
bool ValuesEqual(
    const Value* expected, const Value* actual, std::string* error) {
  if (expected == actual) {
    return true;
  }
  if (expected && !actual) {
    *error = "Expected: " + GetJSON(*expected) + ", actual: NULL";
    return false;
  }
  if (actual && !expected) {
    *error = "Expected: NULL, actual: " + GetJSON(*actual);
    return false;
  }
  if (!expected->Equals(actual)) {
    *error =
        "Expected: " + GetJSON(*expected) + ", actual: " + GetJSON(*actual);
    return false;
  }
  return true;
}

// Returns whether the read result of a storage operation has the expected
// settings.
testing::AssertionResult SettingsEq(
    const char* _1, const char* _2,
    const DictionaryValue& expected,
    const SettingsStorage::ReadResult& actual_result) {
  if (actual_result.HasError()) {
    return testing::AssertionFailure() <<
        "Result has error: " << actual_result.error();
  }

  std::string error;
  if (!ValuesEqual(&expected, &actual_result.settings(), &error)) {
    return testing::AssertionFailure() << error;
  }

  return testing::AssertionSuccess();
}

// Returns whether the write result of a storage operation has the expected
// changes.
testing::AssertionResult ChangesEq(
    const char* _1, const char* _2,
    const SettingChangeList& expected,
    const SettingsStorage::WriteResult& actual_result) {
  if (actual_result.HasError()) {
    return testing::AssertionFailure() <<
        "Result has error: " << actual_result.error();
  }

  const SettingChangeList& actual = actual_result.changes();
  if (expected.size() != actual.size()) {
    return testing::AssertionFailure() <<
        "Actual has wrong size, expecting " << expected.size() <<
        " but was " << actual.size();
  }

  std::map<std::string, linked_ptr<SettingChange> > expected_as_map;
  for (SettingChangeList::const_iterator it = expected.begin();
      it != expected.end(); ++it) {
    expected_as_map[it->key()] =
        linked_ptr<SettingChange>(new SettingChange(*it));
  }

  std::set<std::string> keys_seen;

  for (SettingChangeList::const_iterator it = actual.begin();
      it != actual.end(); ++it) {
    if (keys_seen.count(it->key())) {
      return testing::AssertionFailure() <<
          "Multiple changes seen for key: " << it->key();
    }
    keys_seen.insert(it->key());

    if (!expected_as_map.count(it->key())) {
      return testing::AssertionFailure() <<
          "Actual has unexpected change for key: " << it->key();
    }

    SettingChange expected_change = *expected_as_map[it->key()];
    std::string error;
    if (!ValuesEqual(expected_change.new_value(), it->new_value(), &error)) {
      return testing::AssertionFailure() <<
          "New value for " << it->key() << " was unexpected: " << error;
    }
    if (!ValuesEqual(expected_change.old_value(), it->old_value(), &error)) {
      return testing::AssertionFailure() <<
          "Old value for " << it->key() << " was unexpected: " << error;
    }
  }

  return testing::AssertionSuccess();
}

ExtensionSettingsStorageTest::ExtensionSettingsStorageTest()
    : key1_("foo"),
      key2_("bar"),
      key3_("baz"),
      empty_dict_(new DictionaryValue()),
      dict1_(new DictionaryValue()),
      dict3_(new DictionaryValue()),
      dict12_(new DictionaryValue()),
      dict123_(new DictionaryValue()),
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
  dict3_->Set(key3_, val3_->DeepCopy());
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
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, GetWithSingleValue) {
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key1_, NULL, val1_->DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq,
        changes, storage_->Set(DEFAULTS, key1_, *val1_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key2_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key3_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, GetWithMultipleValues) {
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key1_, NULL, val1_->DeepCopy()));
    changes.push_back(SettingChange(key2_, NULL, val2_->DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, *dict12_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key3_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, RemoveWhenEmpty) {
  EXPECT_PRED_FORMAT2(ChangesEq, SettingChangeList(), storage_->Remove(key1_));

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, RemoveWithSingleValue) {
  storage_->Set(DEFAULTS, *dict1_);
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key1_, val1_->DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(key1_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key2_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list12_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, RemoveWithMultipleValues) {
  storage_->Set(DEFAULTS, *dict123_);
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key3_, val3_->DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(key3_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key3_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(list1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get(list12_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(list13_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get());

  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key1_, val1_->DeepCopy(), NULL));
    changes.push_back(SettingChange(key2_, val2_->DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(list12_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key3_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list12_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list13_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, SetWhenOverwriting) {
  storage_->Set(DEFAULTS, key1_, *val2_);
  {
    SettingChangeList changes;
    changes.push_back(
        SettingChange(key1_, val2_->DeepCopy(), val1_->DeepCopy()));
    changes.push_back(SettingChange(key2_, NULL, val2_->DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, *dict12_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key3_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(list1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get(list12_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(list13_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, ClearWhenEmpty) {
  EXPECT_PRED_FORMAT2(ChangesEq, SettingChangeList(), storage_->Clear());

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, ClearWhenNotEmpty) {
  storage_->Set(DEFAULTS, *dict12_);
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key1_, val1_->DeepCopy(), NULL));
    changes.push_back(SettingChange(key2_, val2_->DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Clear());
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
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

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(dot_key));

  {
    SettingChangeList changes;
    changes.push_back(
        SettingChange(dot_key, NULL, dot_value.DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq,
        changes, storage_->Set(DEFAULTS, dot_key, dot_value));
  }
  EXPECT_PRED_FORMAT2(ChangesEq,
      SettingChangeList(), storage_->Set(DEFAULTS, dot_key, dot_value));

  EXPECT_PRED_FORMAT2(SettingsEq, dot_dict, storage_->Get(dot_key));

  {
    SettingChangeList changes;
    changes.push_back(
        SettingChange(dot_key, dot_value.DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(dot_key));
  }
  EXPECT_PRED_FORMAT2(ChangesEq,
      SettingChangeList(), storage_->Remove(dot_key));
  {
    SettingChangeList changes;
    changes.push_back(
        SettingChange(dot_key, NULL, dot_value.DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, dot_dict));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, dot_dict, storage_->Get(dot_list));
  EXPECT_PRED_FORMAT2(SettingsEq, dot_dict, storage_->Get());

  {
    SettingChangeList changes;
    changes.push_back(
        SettingChange(dot_key, dot_value.DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(dot_list));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(dot_key));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ExtensionSettingsStorageTest, DotsInKeyNamesWithDicts) {
  DictionaryValue outer_dict;
  DictionaryValue* inner_dict = new DictionaryValue();
  outer_dict.Set("foo", inner_dict);
  inner_dict->SetString("bar", "baz");

  {
    SettingChangeList changes;
    changes.push_back(
        SettingChange("foo", NULL, inner_dict->DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq,
        changes, storage_->Set(DEFAULTS, outer_dict));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, outer_dict, storage_->Get("foo"));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get("foo.bar"));
}

TEST_P(ExtensionSettingsStorageTest, ComplexChangedKeysScenarios) {
  // Test:
  //   - Setting over missing/changed/same keys, combinations.
  //   - Removing over missing and present keys, combinations.
  //   - Clearing.
  std::vector<std::string> complex_list;
  DictionaryValue complex_changed_dict;

  storage_->Set(DEFAULTS, key1_, *val1_);
  EXPECT_PRED_FORMAT2(ChangesEq,
      SettingChangeList(), storage_->Set(DEFAULTS, key1_, *val1_));
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(
        key1_, val1_->DeepCopy(), val2_->DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq,
        changes, storage_->Set(DEFAULTS, key1_, *val2_));
  }
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key1_, val2_->DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(key1_));
    EXPECT_PRED_FORMAT2(ChangesEq,
        SettingChangeList(), storage_->Remove(key1_));
  }
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key1_, NULL, val1_->DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq,
        changes, storage_->Set(DEFAULTS, key1_, *val1_));
  }
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key1_, val1_->DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Clear());
    EXPECT_PRED_FORMAT2(ChangesEq, SettingChangeList(), storage_->Clear());
  }

  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key1_, NULL, val1_->DeepCopy()));
    changes.push_back(SettingChange(key2_, NULL, val2_->DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, *dict12_));
    EXPECT_PRED_FORMAT2(ChangesEq,
        SettingChangeList(), storage_->Set(DEFAULTS, *dict12_));
  }
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key3_, NULL, val3_->DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, *dict123_));
  }
  {
    DictionaryValue to_set;
    to_set.Set(key1_, val2_->DeepCopy());
    to_set.Set(key2_, val2_->DeepCopy());
    to_set.Set("asdf", val1_->DeepCopy());
    to_set.Set("qwerty", val3_->DeepCopy());

    SettingChangeList changes;
    changes.push_back(
        SettingChange(key1_, val1_->DeepCopy(), val2_->DeepCopy()));
    changes.push_back(SettingChange("asdf", NULL, val1_->DeepCopy()));
    changes.push_back(
        SettingChange("qwerty", NULL, val3_->DeepCopy()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, to_set));
  }
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key1_, val2_->DeepCopy(), NULL));
    changes.push_back(SettingChange(key2_, val2_->DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(list12_));
  }
  {
    std::vector<std::string> to_remove;
    to_remove.push_back(key1_);
    to_remove.push_back("asdf");

    SettingChangeList changes;
    changes.push_back(SettingChange("asdf", val1_->DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(to_remove));
  }
  {
    SettingChangeList changes;
    changes.push_back(SettingChange(key3_, val3_->DeepCopy(), NULL));
    changes.push_back(
        SettingChange("qwerty", val3_->DeepCopy(), NULL));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Clear());
    EXPECT_PRED_FORMAT2(ChangesEq, SettingChangeList(), storage_->Clear());
  }
}

}  // namespace extensions
