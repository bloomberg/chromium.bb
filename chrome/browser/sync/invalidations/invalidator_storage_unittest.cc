// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "chrome/browser/sync/invalidations/invalidator_storage.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "sync/internal_api/public/base/invalidation_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using syncer::InvalidationStateMap;

namespace browser_sync {

namespace {

const char kSourceKey[] = "source";
const char kNameKey[] = "name";
const char kMaxVersionKey[] = "max-version";
const char kPayloadKey[] = "payload";
const char kCurrentAckHandleKey[] = "current-ack";
const char kExpectedAckHandleKey[] = "expected-ack";

const int kChromeSyncSourceId = 1004;

void GenerateAckHandlesTestHelper(syncer::AckHandleMap* output,
                                  const syncer::AckHandleMap& input) {
  *output = input;
}

}  // namespace

class InvalidatorStorageTest : public testing::Test {
 public:
  InvalidatorStorageTest()
      : kBookmarksId_(kChromeSyncSourceId, "BOOKMARK"),
        kPreferencesId_(kChromeSyncSourceId, "PREFERENCE"),
        kAppNotificationsId_(kChromeSyncSourceId, "APP_NOTIFICATION"),
        kAutofillId_(kChromeSyncSourceId, "AUTOFILL") {}

  void SetUp() {
    InvalidatorStorage::RegisterUserPrefs(pref_service_.registry());
  }

 protected:
  TestingPrefServiceSyncable pref_service_;

  const invalidation::ObjectId kBookmarksId_;
  const invalidation::ObjectId kPreferencesId_;
  const invalidation::ObjectId kAppNotificationsId_;
  const invalidation::ObjectId kAutofillId_;

  MessageLoop loop_;
};

// Set invalidation states for various keys and verify that they are written and
// read back correctly.
TEST_F(InvalidatorStorageTest, SetMaxVersionAndPayload) {
  InvalidatorStorage storage(&pref_service_);

  InvalidationStateMap expected_states;
  EXPECT_EQ(expected_states, storage.GetAllInvalidationStates());

  expected_states[kBookmarksId_].version = 2;
  expected_states[kBookmarksId_].payload = "hello";
  storage.SetMaxVersionAndPayload(kBookmarksId_, 2, "hello");
  EXPECT_EQ(expected_states, storage.GetAllInvalidationStates());

  expected_states[kPreferencesId_].version = 5;
  storage.SetMaxVersionAndPayload(kPreferencesId_, 5, std::string());
  EXPECT_EQ(expected_states, storage.GetAllInvalidationStates());

  expected_states[kAppNotificationsId_].version = 3;
  expected_states[kAppNotificationsId_].payload = "world";
  storage.SetMaxVersionAndPayload(kAppNotificationsId_, 3, "world");
  EXPECT_EQ(expected_states, storage.GetAllInvalidationStates());

  expected_states[kAppNotificationsId_].version = 4;
  expected_states[kAppNotificationsId_].payload = "again";
  storage.SetMaxVersionAndPayload(kAppNotificationsId_, 4, "again");
  EXPECT_EQ(expected_states, storage.GetAllInvalidationStates());
}

// Forgetting an entry should cause that entry to be deleted.
TEST_F(InvalidatorStorageTest, Forget) {
  InvalidatorStorage storage(&pref_service_);
  EXPECT_TRUE(storage.GetAllInvalidationStates().empty());

  InvalidationStateMap expected_states;
  expected_states[kBookmarksId_].version = 2;
  expected_states[kBookmarksId_].payload = "a";
  expected_states[kPreferencesId_].version = 5;
  expected_states[kPreferencesId_].payload = "b";
  storage.SetMaxVersionAndPayload(kBookmarksId_, 2, "a");
  storage.SetMaxVersionAndPayload(kPreferencesId_, 5, "b");
  EXPECT_EQ(expected_states, storage.GetAllInvalidationStates());

  expected_states.erase(kPreferencesId_);
  syncer::ObjectIdSet to_forget;
  to_forget.insert(kPreferencesId_);
  storage.Forget(to_forget);
  EXPECT_EQ(expected_states, storage.GetAllInvalidationStates());
}

// Clearing the storage should erase all version map entries, bootstrap data,
// and the client ID.
TEST_F(InvalidatorStorageTest, Clear) {
  InvalidatorStorage storage(&pref_service_);
  EXPECT_TRUE(storage.GetAllInvalidationStates().empty());
  EXPECT_TRUE(storage.GetBootstrapData().empty());
  EXPECT_TRUE(storage.GetInvalidatorClientId().empty());

  storage.SetInvalidatorClientId("fake_id");
  EXPECT_EQ("fake_id", storage.GetInvalidatorClientId());

  storage.SetBootstrapData("test");
  EXPECT_EQ("test", storage.GetBootstrapData());

  {
    InvalidationStateMap expected_states;
    expected_states[kAppNotificationsId_].version = 3;
    storage.SetMaxVersionAndPayload(kAppNotificationsId_, 3, std::string());
    EXPECT_EQ(expected_states, storage.GetAllInvalidationStates());
  }

  storage.Clear();

  EXPECT_TRUE(storage.GetAllInvalidationStates().empty());
  EXPECT_TRUE(storage.GetBootstrapData().empty());
  EXPECT_TRUE(storage.GetInvalidatorClientId().empty());
}

TEST_F(InvalidatorStorageTest, SerializeEmptyMap) {
  InvalidationStateMap empty_map;
  base::ListValue list;
  InvalidatorStorage::SerializeToList(empty_map, &list);
  EXPECT_TRUE(list.empty());
}

// Make sure we don't choke on a variety of malformed input.
TEST_F(InvalidatorStorageTest, DeserializeFromListInvalidFormat) {
  InvalidationStateMap map;
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
  EXPECT_EQ(20, map[valid_id].version);
}

// Tests behavior when there are duplicate entries for a single key. The value
// of the last entry with that key should be used in the version map.
TEST_F(InvalidatorStorageTest, DeserializeFromListWithDuplicates) {
  InvalidationStateMap map;
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
  EXPECT_EQ(10, map[kAutofillId_].version);
  EXPECT_EQ(15, map[kBookmarksId_].version);
}

TEST_F(InvalidatorStorageTest, DeserializeFromEmptyList) {
  InvalidationStateMap map;
  base::ListValue list;
  InvalidatorStorage::DeserializeFromList(list, &map);
  EXPECT_TRUE(map.empty());
}

// Tests that deserializing a well-formed value results in the expected state
// map.
TEST_F(InvalidatorStorageTest, DeserializeFromListBasic) {
  InvalidationStateMap map;
  base::ListValue list;
  DictionaryValue* value;
  syncer::AckHandle ack_handle_1 = syncer::AckHandle::CreateUnique();
  syncer::AckHandle ack_handle_2 = syncer::AckHandle::CreateUnique();

  value = new DictionaryValue();
  value->SetString(kSourceKey,
                   base::IntToString(kAppNotificationsId_.source()));
  value->SetString(kNameKey, kAppNotificationsId_.name());
  value->SetString(kMaxVersionKey, "20");
  value->SetString(kPayloadKey, "testing");
  value->Set(kCurrentAckHandleKey, ack_handle_1.ToValue().release());
  value->Set(kExpectedAckHandleKey, ack_handle_2.ToValue().release());
  list.Append(value);

  InvalidatorStorage::DeserializeFromList(list, &map);
  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(20, map[kAppNotificationsId_].version);
  EXPECT_EQ("testing", map[kAppNotificationsId_].payload);
  EXPECT_THAT(map[kAppNotificationsId_].current, Eq(ack_handle_1));
  EXPECT_THAT(map[kAppNotificationsId_].expected, Eq(ack_handle_2));
}

// Tests that deserializing well-formed values when optional parameters are
// omitted works.
TEST_F(InvalidatorStorageTest, DeserializeFromListMissingOptionalValues) {
  InvalidationStateMap map;
  base::ListValue list;
  DictionaryValue* value;
  syncer::AckHandle ack_handle = syncer::AckHandle::CreateUnique();

  // Payload missing because of an upgrade from a previous browser version that
  // didn't set the field.
  value = new DictionaryValue();
  value->SetString(kSourceKey, base::IntToString(kAutofillId_.source()));
  value->SetString(kNameKey, kAutofillId_.name());
  value->SetString(kMaxVersionKey, "10");
  list.Append(value);
  // A crash between SetMaxVersion() and a callback from GenerateAckHandles()
  // could result in this state.
  value = new DictionaryValue();
  value->SetString(kSourceKey, base::IntToString(kBookmarksId_.source()));
  value->SetString(kNameKey, kBookmarksId_.name());
  value->SetString(kMaxVersionKey, "15");
  value->SetString(kPayloadKey, "hello");
  list.Append(value);
  // Never acknowledged, so current ack handle is unset.
  value = new DictionaryValue();
  value->SetString(kSourceKey, base::IntToString(kPreferencesId_.source()));
  value->SetString(kNameKey, kPreferencesId_.name());
  value->SetString(kMaxVersionKey, "20");
  value->SetString(kPayloadKey, "world");
  value->Set(kExpectedAckHandleKey, ack_handle.ToValue().release());
  list.Append(value);

  InvalidatorStorage::DeserializeFromList(list, &map);
  EXPECT_EQ(3U, map.size());

  EXPECT_EQ(10, map[kAutofillId_].version);
  EXPECT_EQ("", map[kAutofillId_].payload);
  EXPECT_FALSE(map[kAutofillId_].current.IsValid());
  EXPECT_FALSE(map[kAutofillId_].expected.IsValid());

  EXPECT_EQ(15, map[kBookmarksId_].version);
  EXPECT_EQ("hello", map[kBookmarksId_].payload);
  EXPECT_FALSE(map[kBookmarksId_].current.IsValid());
  EXPECT_FALSE(map[kBookmarksId_].expected.IsValid());

  EXPECT_EQ(20, map[kPreferencesId_].version);
  EXPECT_EQ("world", map[kPreferencesId_].payload);
  EXPECT_FALSE(map[kPreferencesId_].current.IsValid());
  EXPECT_THAT(map[kPreferencesId_].expected, Eq(ack_handle));
}

// Tests for legacy deserialization code.
TEST_F(InvalidatorStorageTest, DeserializeMapOutOfRange) {
  InvalidationStateMap map;
  base::DictionaryValue dict_with_out_of_range_type;

  dict_with_out_of_range_type.SetString(
      base::IntToString(syncer::TOP_LEVEL_FOLDER), "100");
  dict_with_out_of_range_type.SetString(
      base::IntToString(syncer::BOOKMARKS), "5");

  InvalidatorStorage::DeserializeMap(&dict_with_out_of_range_type, &map);

  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(5, map[kBookmarksId_].version);
}

TEST_F(InvalidatorStorageTest, DeserializeMapInvalidFormat) {
  InvalidationStateMap map;
  base::DictionaryValue dict_with_invalid_format;

  dict_with_invalid_format.SetString("whoops", "5");
  dict_with_invalid_format.SetString("ohnoes", "whoops");
  dict_with_invalid_format.SetString(
      base::IntToString(syncer::BOOKMARKS), "ohnoes");
  dict_with_invalid_format.SetString(
      base::IntToString(syncer::AUTOFILL), "10");

  InvalidatorStorage::DeserializeMap(&dict_with_invalid_format, &map);

  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(10, map[kAutofillId_].version);
}

TEST_F(InvalidatorStorageTest, DeserializeMapEmptyDictionary) {
  InvalidationStateMap map;
  base::DictionaryValue dict;
  InvalidatorStorage::DeserializeMap(&dict, &map);
  EXPECT_TRUE(map.empty());
}

TEST_F(InvalidatorStorageTest, DeserializeMapBasic) {
  InvalidationStateMap map;
  base::DictionaryValue dict;

  dict.SetString(base::IntToString(syncer::AUTOFILL), "10");
  dict.SetString(base::IntToString(syncer::BOOKMARKS), "15");

  InvalidatorStorage::DeserializeMap(&dict, &map);
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ(10, map[kAutofillId_].version);
  EXPECT_EQ(15, map[kBookmarksId_].version);
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
  InvalidationStateMap map;
  const base::ListValue* list =
      pref_service_.GetList(prefs::kInvalidatorMaxInvalidationVersions);
  InvalidatorStorage::DeserializeFromList(*list, &map);

  EXPECT_EQ(3U, map.size());
  EXPECT_EQ(10, map[kAutofillId_].version);
  EXPECT_EQ(32, map[kBookmarksId_].version);
  EXPECT_EQ(54, map[kPreferencesId_].version);
}

TEST_F(InvalidatorStorageTest, SetGetNotifierClientId) {
  InvalidatorStorage storage(&pref_service_);
  const std::string client_id("fK6eDzAIuKqx9A4+93bljg==");

  storage.SetInvalidatorClientId(client_id);
  EXPECT_EQ(client_id, storage.GetInvalidatorClientId());
}

TEST_F(InvalidatorStorageTest, SetGetBootstrapData) {
  InvalidatorStorage storage(&pref_service_);
  const std::string mess("n\0tK\0\0l\344", 8);
  ASSERT_FALSE(IsStringUTF8(mess));

  storage.SetBootstrapData(mess);
  EXPECT_EQ(mess, storage.GetBootstrapData());
}

// Test that we correctly generate ack handles, acknowledge them, and persist
// them.
TEST_F(InvalidatorStorageTest, GenerateAckHandlesAndAcknowledge) {
  InvalidatorStorage storage(&pref_service_);
  syncer::ObjectIdSet ids;
  InvalidationStateMap state_map;
  syncer::AckHandleMap ack_handle_map;
  syncer::AckHandleMap::const_iterator it;

  // Test that it works as expected if the key doesn't already exist in the map,
  // e.g. the first invalidation received for the object ID was not for a
  // specific version.
  ids.insert(kAutofillId_);
  storage.GenerateAckHandles(
      ids, base::MessageLoopProxy::current(),
      base::Bind(&GenerateAckHandlesTestHelper, &ack_handle_map));
  loop_.RunUntilIdle();
  EXPECT_EQ(1U, ack_handle_map.size());
  it = ack_handle_map.find(kAutofillId_);
  // Android STL appears to be buggy and causes gtest's IsContainerTest<> to
  // treat an iterator as a STL container so we use != instead of ASSERT_NE.
  ASSERT_TRUE(ack_handle_map.end() != it);
  EXPECT_TRUE(it->second.IsValid());
  state_map[kAutofillId_].expected = it->second;
  EXPECT_EQ(state_map, storage.GetAllInvalidationStates());

  storage.Acknowledge(kAutofillId_, it->second);
  state_map[kAutofillId_].current = it->second;
  EXPECT_EQ(state_map, storage.GetAllInvalidationStates());

  ids.clear();

  // Test that it works as expected if the key already exists.
  state_map[kBookmarksId_].version = 11;
  state_map[kBookmarksId_].payload = "hello";
  storage.SetMaxVersionAndPayload(kBookmarksId_, 11, "hello");
  EXPECT_EQ(state_map, storage.GetAllInvalidationStates());
  ids.insert(kBookmarksId_);
  storage.GenerateAckHandles(
      ids, base::MessageLoopProxy::current(),
      base::Bind(&GenerateAckHandlesTestHelper, &ack_handle_map));
  loop_.RunUntilIdle();
  EXPECT_EQ(1U, ack_handle_map.size());
  it = ack_handle_map.find(kBookmarksId_);
  ASSERT_TRUE(ack_handle_map.end() != it);
  EXPECT_TRUE(it->second.IsValid());
  state_map[kBookmarksId_].expected = it->second;
  EXPECT_EQ(state_map, storage.GetAllInvalidationStates());

  storage.Acknowledge(kBookmarksId_, it->second);
  state_map[kBookmarksId_].current = it->second;
  EXPECT_EQ(state_map, storage.GetAllInvalidationStates());

  // Finally, test that the ack handles are updated if we're asked to generate
  // another ack handle for the same object ID.
  state_map[kBookmarksId_].version = 12;
  state_map[kBookmarksId_].payload = "world";
  storage.SetMaxVersionAndPayload(kBookmarksId_, 12, "world");
  EXPECT_EQ(state_map, storage.GetAllInvalidationStates());
  ids.insert(kBookmarksId_);
  storage.GenerateAckHandles(
      ids, base::MessageLoopProxy::current(),
      base::Bind(&GenerateAckHandlesTestHelper, &ack_handle_map));
  loop_.RunUntilIdle();
  EXPECT_EQ(1U, ack_handle_map.size());
  it = ack_handle_map.find(kBookmarksId_);
  ASSERT_TRUE(ack_handle_map.end() != it);
  EXPECT_TRUE(it->second.IsValid());
  state_map[kBookmarksId_].expected = it->second;
  EXPECT_EQ(state_map, storage.GetAllInvalidationStates());

  storage.Acknowledge(kBookmarksId_, it->second);
  state_map[kBookmarksId_].current = it->second;
  EXPECT_EQ(state_map, storage.GetAllInvalidationStates());
}

}  // namespace browser_sync
