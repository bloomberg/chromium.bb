// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/sync/invalidations/invalidator_storage.h"

#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::InvalidationVersionMap;

namespace browser_sync {

namespace {

const char kSourceKey[] = "source";
const char kNameKey[] = "name";
const char kMaxVersionKey[] = "max-version";

const int kChromeSyncSourceId = 1004;

}  // namespace

class InvalidatorStorageTest : public testing::Test {
 public:
  InvalidatorStorageTest()
      : kBookmarksId_(kChromeSyncSourceId, "BOOKMARK"),
        kPreferencesId_(kChromeSyncSourceId, "PREFERENCE"),
        kAppNotificationsId_(kChromeSyncSourceId, "APP_NOTIFICATION"),
        kAutofillId_(kChromeSyncSourceId, "AUTOFILL") {}

 protected:
  TestingPrefService pref_service_;

  const invalidation::ObjectId kBookmarksId_;
  const invalidation::ObjectId kPreferencesId_;
  const invalidation::ObjectId kAppNotificationsId_;
  const invalidation::ObjectId kAutofillId_;

 private:
  MessageLoop loop_;
};

// Set max versions for various keys and verify that they are written and read
// back correctly.
TEST_F(InvalidatorStorageTest, MaxInvalidationVersions) {
  InvalidatorStorage storage(&pref_service_);

  InvalidationVersionMap expected_max_versions;
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());

  expected_max_versions[kBookmarksId_] = 2;
  storage.SetMaxVersion(kBookmarksId_, 2);
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());

  expected_max_versions[kPreferencesId_] = 5;
  storage.SetMaxVersion(kPreferencesId_, 5);
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());

  expected_max_versions[kAppNotificationsId_] = 3;
  storage.SetMaxVersion(kAppNotificationsId_, 3);
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());

  expected_max_versions[kAppNotificationsId_] = 4;
  storage.SetMaxVersion(kAppNotificationsId_, 4);
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());
}

// Forgetting an entry should cause that entry to be deleted.
TEST_F(InvalidatorStorageTest, Forget) {
  InvalidatorStorage storage(&pref_service_);
  EXPECT_TRUE(storage.GetAllMaxVersions().empty());

  InvalidationVersionMap expected_max_versions;
  expected_max_versions[kBookmarksId_] = 2;
  expected_max_versions[kPreferencesId_] = 5;
  storage.SetMaxVersion(kBookmarksId_, 2);
  storage.SetMaxVersion(kPreferencesId_, 5);
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());

  expected_max_versions.erase(kPreferencesId_);
  syncer::ObjectIdSet to_forget;
  to_forget.insert(kPreferencesId_);
  storage.Forget(to_forget);
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());
}

// Clearing the storage should result in an empty version map.
TEST_F(InvalidatorStorageTest, Clear) {
  InvalidatorStorage storage(&pref_service_);
  EXPECT_TRUE(storage.GetAllMaxVersions().empty());
  EXPECT_TRUE(storage.GetInvalidationState().empty());

  storage.SetInvalidationState("test");
  EXPECT_EQ("test", storage.GetInvalidationState());
  {
    InvalidationVersionMap expected_max_versions;
    expected_max_versions[kAppNotificationsId_] = 3;
    storage.SetMaxVersion(kAppNotificationsId_, 3);
    EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());
  }

  storage.Clear();

  EXPECT_TRUE(storage.GetAllMaxVersions().empty());
  EXPECT_TRUE(storage.GetInvalidationState().empty());
}

TEST_F(InvalidatorStorageTest, SerializeEmptyMap) {
  InvalidationVersionMap empty_map;
  base::ListValue list;
  InvalidatorStorage::SerializeToList(empty_map, &list);
  EXPECT_TRUE(list.empty());
}

// Make sure we don't choke on a variety of malformed input.
TEST_F(InvalidatorStorageTest, DeserializeFromListInvalidFormat) {
  InvalidationVersionMap map;
  base::ListValue list_with_invalid_format;
  DictionaryValue* value;

  // The various cases below use distinct values to make it easier to track down
  // failures.
  value = new DictionaryValue();
  list_with_invalid_format.Append(value);

  value = new DictionaryValue();
  value->SetString("completely", "invalid");
  list_with_invalid_format.Append(value);

  // Missing two required fields
  value = new DictionaryValue();
  value->SetString(kSourceKey, "10");
  list_with_invalid_format.Append(value);

  value = new DictionaryValue();
  value->SetString(kNameKey, "missing source and version");
  list_with_invalid_format.Append(value);

  value = new DictionaryValue();
  value->SetString(kMaxVersionKey, "3");
  list_with_invalid_format.Append(value);

  // Missing one required field
  value = new DictionaryValue();
  value->SetString(kSourceKey, "14");
  value->SetString(kNameKey, "missing version");
  list_with_invalid_format.Append(value);

  value = new DictionaryValue();
  value->SetString(kSourceKey, "233");
  value->SetString(kMaxVersionKey, "5");
  list_with_invalid_format.Append(value);

  value = new DictionaryValue();
  value->SetString(kNameKey, "missing source");
  value->SetString(kMaxVersionKey, "25");
  list_with_invalid_format.Append(value);

  // Invalid values in fields
  value = new DictionaryValue();
  value->SetString(kSourceKey, "a");
  value->SetString(kNameKey, "bad source");
  value->SetString(kMaxVersionKey, "12");
  list_with_invalid_format.Append(value);

  value = new DictionaryValue();
  value->SetString(kSourceKey, "1");
  value->SetString(kNameKey, "bad max version");
  value->SetString(kMaxVersionKey, "a");
  list_with_invalid_format.Append(value);

  // And finally something that should work.
  invalidation::ObjectId valid_id(42, "this should work");
  value = new DictionaryValue();
  value->SetString(kSourceKey, "42");
  value->SetString(kNameKey, valid_id.name());
  value->SetString(kMaxVersionKey, "20");
  list_with_invalid_format.Append(value);

  InvalidatorStorage::DeserializeFromList(list_with_invalid_format, &map);

  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(20, map[valid_id]);
}

// Tests behavior when there are duplicate entries for a single key. The value
// of the last entry with that key should be used in the version map.
TEST_F(InvalidatorStorageTest, DeserializeFromListWithDuplicates) {
  InvalidationVersionMap map;
  base::ListValue list;
  DictionaryValue* value;

  value = new DictionaryValue();
  value->SetString(kSourceKey, base::IntToString(kBookmarksId_.source()));
  value->SetString(kNameKey, kBookmarksId_.name());
  value->SetString(kMaxVersionKey, "20");
  list.Append(value);
  value = new DictionaryValue();
  value->SetString(kSourceKey, base::IntToString(kAutofillId_.source()));
  value->SetString(kNameKey, kAutofillId_.name());
  value->SetString(kMaxVersionKey, "10");
  list.Append(value);
  value = new DictionaryValue();
  value->SetString(kSourceKey, base::IntToString(kBookmarksId_.source()));
  value->SetString(kNameKey, kBookmarksId_.name());
  value->SetString(kMaxVersionKey, "15");
  list.Append(value);

  InvalidatorStorage::DeserializeFromList(list, &map);
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ(10, map[kAutofillId_]);
  EXPECT_EQ(15, map[kBookmarksId_]);
}

TEST_F(InvalidatorStorageTest, DeserializeFromEmptyList) {
  InvalidationVersionMap map;
  base::ListValue list;
  InvalidatorStorage::DeserializeFromList(list, &map);
  EXPECT_TRUE(map.empty());
}

// Tests that deserializing a well-formed value results in the expected version
// map.
TEST_F(InvalidatorStorageTest, DeserializeFromListBasic) {
  InvalidationVersionMap map;
  base::ListValue list;
  DictionaryValue* value;

  value = new DictionaryValue();
  value->SetString(kSourceKey, base::IntToString(kAutofillId_.source()));
  value->SetString(kNameKey, kAutofillId_.name());
  value->SetString(kMaxVersionKey, "10");
  list.Append(value);
  value = new DictionaryValue();
  value->SetString(kSourceKey, base::IntToString(kBookmarksId_.source()));
  value->SetString(kNameKey, kBookmarksId_.name());
  value->SetString(kMaxVersionKey, "15");
  list.Append(value);

  InvalidatorStorage::DeserializeFromList(list, &map);
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ(10, map[kAutofillId_]);
  EXPECT_EQ(15, map[kBookmarksId_]);
}

// Tests for legacy deserialization code.
TEST_F(InvalidatorStorageTest, DeserializeMapOutOfRange) {
  InvalidationVersionMap map;
  base::DictionaryValue dict_with_out_of_range_type;

  dict_with_out_of_range_type.SetString(
      base::IntToString(syncer::TOP_LEVEL_FOLDER), "100");
  dict_with_out_of_range_type.SetString(
      base::IntToString(syncer::BOOKMARKS), "5");

  InvalidatorStorage::DeserializeMap(&dict_with_out_of_range_type, &map);

  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(5, map[kBookmarksId_]);
}

TEST_F(InvalidatorStorageTest, DeserializeMapInvalidFormat) {
  InvalidationVersionMap map;
  base::DictionaryValue dict_with_invalid_format;

  dict_with_invalid_format.SetString("whoops", "5");
  dict_with_invalid_format.SetString("ohnoes", "whoops");
  dict_with_invalid_format.SetString(
      base::IntToString(syncer::BOOKMARKS), "ohnoes");
  dict_with_invalid_format.SetString(
      base::IntToString(syncer::AUTOFILL), "10");

  InvalidatorStorage::DeserializeMap(&dict_with_invalid_format, &map);

  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(10, map[kAutofillId_]);
}

TEST_F(InvalidatorStorageTest, DeserializeMapEmptyDictionary) {
  InvalidationVersionMap map;
  base::DictionaryValue dict;
  InvalidatorStorage::DeserializeMap(&dict, &map);
  EXPECT_TRUE(map.empty());
}

TEST_F(InvalidatorStorageTest, DeserializeMapBasic) {
  InvalidationVersionMap map;
  base::DictionaryValue dict;

  dict.SetString(base::IntToString(syncer::AUTOFILL), "10");
  dict.SetString(base::IntToString(syncer::BOOKMARKS), "15");

  InvalidatorStorage::DeserializeMap(&dict, &map);
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ(10, map[kAutofillId_]);
  EXPECT_EQ(15, map[kBookmarksId_]);
}

// Test that the migration code for the legacy preference works as expected.
// Migration should happen on construction of InvalidatorStorage.
TEST_F(InvalidatorStorageTest, MigrateLegacyPreferences) {
  base::DictionaryValue* legacy_dict = new DictionaryValue;
  legacy_dict->SetString(base::IntToString(syncer::AUTOFILL), "10");
  legacy_dict->SetString(base::IntToString(syncer::BOOKMARKS), "32");
  legacy_dict->SetString(base::IntToString(syncer::PREFERENCES), "54");
  pref_service_.SetUserPref(prefs::kSyncMaxInvalidationVersions, legacy_dict);
  InvalidatorStorage storage(&pref_service_);

  // Legacy pref should be cleared.
  const base::DictionaryValue* dict =
      pref_service_.GetDictionary(prefs::kSyncMaxInvalidationVersions);
  EXPECT_TRUE(dict->empty());

  // Validate the new pref is set correctly.
  InvalidationVersionMap map;
  const base::ListValue* list =
      pref_service_.GetList(prefs::kInvalidatorMaxInvalidationVersions);
  InvalidatorStorage::DeserializeFromList(*list, &map);

  EXPECT_EQ(3U, map.size());
  EXPECT_EQ(10, map[kAutofillId_]);
  EXPECT_EQ(32, map[kBookmarksId_]);
  EXPECT_EQ(54, map[kPreferencesId_]);
}

TEST_F(InvalidatorStorageTest, SetGetInvalidationState) {
  InvalidatorStorage storage(&pref_service_);
  const std::string mess("n\0tK\0\0l\344", 8);
  ASSERT_FALSE(IsStringUTF8(mess));

  storage.SetInvalidationState(mess);
  EXPECT_EQ(mess, storage.GetInvalidationState());
}

}  // namespace browser_sync
