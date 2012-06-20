// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/sync/invalidations/invalidator_storage.h"

#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/test/base/testing_pref_service.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using csync::InvalidationVersionMap;

namespace browser_sync {

class InvalidatorStorageTest : public testing::Test {
 protected:
  TestingPrefService pref_service_;

 private:
  MessageLoop loop_;
};

TEST_F(InvalidatorStorageTest, MaxInvalidationVersions) {
  InvalidatorStorage storage(&pref_service_);

  InvalidationVersionMap expected_max_versions;
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());

  expected_max_versions[syncable::BOOKMARKS] = 2;
  storage.SetMaxVersion(syncable::BOOKMARKS, 2);
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());

  expected_max_versions[syncable::PREFERENCES] = 5;
  storage.SetMaxVersion(syncable::PREFERENCES, 5);
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());

  expected_max_versions[syncable::APP_NOTIFICATIONS] = 3;
  storage.SetMaxVersion(syncable::APP_NOTIFICATIONS, 3);
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());

  expected_max_versions[syncable::APP_NOTIFICATIONS] = 4;
  storage.SetMaxVersion(syncable::APP_NOTIFICATIONS, 4);
  EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());
}

TEST_F(InvalidatorStorageTest, Clear) {
  InvalidatorStorage storage(&pref_service_);
  EXPECT_TRUE(storage.GetAllMaxVersions().empty());
  EXPECT_TRUE(storage.GetInvalidationState().empty());

  storage.SetInvalidationState("test");
  EXPECT_EQ("test", storage.GetInvalidationState());
  {
    InvalidationVersionMap expected_max_versions;
    expected_max_versions[syncable::APP_NOTIFICATIONS] = 3;
    storage.SetMaxVersion(syncable::APP_NOTIFICATIONS, 3);
    EXPECT_EQ(expected_max_versions, storage.GetAllMaxVersions());
  }

  storage.Clear();

  EXPECT_TRUE(storage.GetAllMaxVersions().empty());
  EXPECT_TRUE(storage.GetInvalidationState().empty());
}

TEST_F(InvalidatorStorageTest, SerializeEmptyMap) {
  InvalidationVersionMap empty_map;
  base::DictionaryValue dict;
  InvalidatorStorage::SerializeMap(empty_map, &dict);
  EXPECT_TRUE(dict.empty());
}

TEST_F(InvalidatorStorageTest, DeserializeOutOfRange) {
  InvalidationVersionMap map;
  base::DictionaryValue dict_with_out_of_range_type;

  dict_with_out_of_range_type.SetString(
      base::IntToString(syncable::TOP_LEVEL_FOLDER), "100");
  dict_with_out_of_range_type.SetString(
      base::IntToString(syncable::BOOKMARKS), "5");

  InvalidatorStorage::DeserializeMap(&dict_with_out_of_range_type, &map);

  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(5, map[syncable::BOOKMARKS]);
}

TEST_F(InvalidatorStorageTest, DeserializeInvalidFormat) {
  InvalidationVersionMap map;
  base::DictionaryValue dict_with_invalid_format;

  dict_with_invalid_format.SetString("whoops", "5");
  dict_with_invalid_format.SetString("ohnoes", "whoops");
  dict_with_invalid_format.SetString(
      base::IntToString(syncable::BOOKMARKS), "ohnoes");
  dict_with_invalid_format.SetString(
      base::IntToString(syncable::AUTOFILL), "10");

  InvalidatorStorage::DeserializeMap(&dict_with_invalid_format, &map);

  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(10, map[syncable::AUTOFILL]);
}

TEST_F(InvalidatorStorageTest, DeserializeEmptyDictionary) {
  InvalidationVersionMap map;
  base::DictionaryValue dict;
  InvalidatorStorage::DeserializeMap(&dict, &map);
  EXPECT_TRUE(map.empty());
}

TEST_F(InvalidatorStorageTest, DeserializeBasic) {
  InvalidationVersionMap map;
  base::DictionaryValue dict;

  dict.SetString(base::IntToString(syncable::AUTOFILL), "10");
  dict.SetString(base::IntToString(syncable::BOOKMARKS), "15");

  InvalidatorStorage::DeserializeMap(&dict, &map);
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ(10, map[syncable::AUTOFILL]);
  EXPECT_EQ(15, map[syncable::BOOKMARKS]);
}

TEST_F(InvalidatorStorageTest, SetGetInvalidationState) {
  InvalidatorStorage storage(&pref_service_);
  const std::string mess("n\0tK\0\0l\344", 8);
  ASSERT_FALSE(IsStringUTF8(mess));

  storage.SetInvalidationState(mess);
  EXPECT_EQ(mess, storage.GetInvalidationState());
}

}  // namespace browser_sync
