// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_store_impl.h"

#include <string>

#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrefHashStoreImplTest, AtomicHashStoreAndCheck) {
  base::StringValue string_1("string1");
  base::StringValue string_2("string2");

  TestingPrefServiceSimple local_state;
  PrefHashStoreImpl::RegisterPrefs(local_state.registry());

  // 32 NULL bytes is the seed that was used to generate the legacy hash.
  PrefHashStoreImpl pref_hash_store(
      "store_id", std::string(32, 0), "device_id", &local_state);

  // Only NULL should be trusted in the absence of a hash.
  EXPECT_EQ(PrefHashStore::UNTRUSTED_UNKNOWN_VALUE,
            pref_hash_store.CheckValue("path1", &string_1));
  EXPECT_EQ(PrefHashStore::TRUSTED_UNKNOWN_VALUE,
            pref_hash_store.CheckValue("path1", NULL));

  pref_hash_store.StoreHash("path1", &string_1);
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckValue("path1", &string_1));
  EXPECT_EQ(PrefHashStore::CLEARED, pref_hash_store.CheckValue("path1", NULL));
  pref_hash_store.StoreHash("path1", NULL);
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckValue("path1", NULL));
  EXPECT_EQ(PrefHashStore::CHANGED,
            pref_hash_store.CheckValue("path1", &string_2));

  base::DictionaryValue dict;
  dict.Set("a", new base::StringValue("foo"));
  dict.Set("d", new base::StringValue("bad"));
  dict.Set("b", new base::StringValue("bar"));
  dict.Set("c", new base::StringValue("baz"));

  {
    // Manually shove in a legacy hash.
    DictionaryPrefUpdate update(&local_state, prefs::kProfilePreferenceHashes);
    base::DictionaryValue* child_dictionary = NULL;
    ASSERT_TRUE(update->GetDictionary("store_id", &child_dictionary));
    child_dictionary->SetString(
        "path1",
        "C503FB7C65EEFD5C07185F616A0AA67923C069909933F362022B1F187E73E9A2");
  }
  EXPECT_EQ(PrefHashStore::MIGRATED,
            pref_hash_store.CheckValue("path1", &dict));
  pref_hash_store.StoreHash("path1", &dict);
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckValue("path1", &dict));

  // |pref_hash_store2| should trust its initial hashes dictionary and thus
  // trust new unknown values.
  PrefHashStoreImpl pref_hash_store2(
      "store_id", std::string(32, 0), "device_id", &local_state);
  EXPECT_EQ(PrefHashStore::TRUSTED_UNKNOWN_VALUE,
            pref_hash_store2.CheckValue("new_path", &string_1));
  EXPECT_EQ(PrefHashStore::TRUSTED_UNKNOWN_VALUE,
            pref_hash_store2.CheckValue("new_path", &string_2));
  EXPECT_EQ(PrefHashStore::TRUSTED_UNKNOWN_VALUE,
            pref_hash_store2.CheckValue("new_path", NULL));

  {
    // Manually corrupt the hash of hashes for "store_id".
    DictionaryPrefUpdate update(&local_state, prefs::kProfilePreferenceHashes);
    base::DictionaryValue* hash_of_hashes_dict = NULL;
    ASSERT_TRUE(update->GetDictionaryWithoutPathExpansion(
        internals::kHashOfHashesPref, &hash_of_hashes_dict));
    hash_of_hashes_dict->SetString("store_id", std::string(64, 'A'));
    // This shouldn't have increased the number of existing hash of hashes.
    ASSERT_EQ(1U, hash_of_hashes_dict->size());
  }

  // |pref_hash_store3| should no longer trust its initial hashes dictionary and
  // thus shouldn't trust non-NULL unknown values.
  PrefHashStoreImpl pref_hash_store3(
      "store_id", std::string(32, 0), "device_id", &local_state);
  EXPECT_EQ(PrefHashStore::UNTRUSTED_UNKNOWN_VALUE,
            pref_hash_store3.CheckValue("new_path", &string_1));
  EXPECT_EQ(PrefHashStore::UNTRUSTED_UNKNOWN_VALUE,
            pref_hash_store3.CheckValue("new_path", &string_2));
  EXPECT_EQ(PrefHashStore::TRUSTED_UNKNOWN_VALUE,
            pref_hash_store3.CheckValue("new_path", NULL));
}

TEST(PrefHashStoreImplTest, SplitHashStoreAndCheck) {
  base::DictionaryValue dict;
  dict.Set("a", new base::StringValue("to be replaced"));
  dict.Set("b", new base::StringValue("same"));
  dict.Set("o", new base::StringValue("old"));

  base::DictionaryValue modified_dict;
  modified_dict.Set("a", new base::StringValue("replaced"));
  modified_dict.Set("b", new base::StringValue("same"));
  modified_dict.Set("c", new base::StringValue("new"));

  base::DictionaryValue empty_dict;

  TestingPrefServiceSimple local_state;
  PrefHashStoreImpl::RegisterPrefs(local_state.registry());

  PrefHashStoreImpl pref_hash_store(
      "store_id", std::string(32, 0), "device_id", &local_state);

  std::vector<std::string> invalid_keys;

  // No hashes stored yet and hashes dictionary is empty (and thus not trusted).
  EXPECT_EQ(PrefHashStore::UNTRUSTED_UNKNOWN_VALUE,
            pref_hash_store.CheckSplitValue("path1", &dict, &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());

  pref_hash_store.StoreSplitHash("path1", &dict);

  // Verify match post storage.
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckSplitValue("path1", &dict, &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());

  // Verify new path is still unknown.
  EXPECT_EQ(PrefHashStore::UNTRUSTED_UNKNOWN_VALUE,
            pref_hash_store.CheckSplitValue("path2", &dict, &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());

  // Verify NULL or empty dicts are declared as having been cleared.
  EXPECT_EQ(PrefHashStore::CLEARED,
            pref_hash_store.CheckSplitValue("path1", NULL, &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());
  EXPECT_EQ(PrefHashStore::CLEARED,
            pref_hash_store.CheckSplitValue("path1", &empty_dict,
                                            &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());

  // Verify changes are properly detected.
  EXPECT_EQ(PrefHashStore::CHANGED,
            pref_hash_store.CheckSplitValue("path1", &modified_dict,
                                            &invalid_keys));
  std::vector<std::string> expected_invalid_keys1;
  expected_invalid_keys1.push_back("a");
  expected_invalid_keys1.push_back("c");
  EXPECT_EQ(expected_invalid_keys1, invalid_keys);
  invalid_keys.clear();

  // Verify |dict| still matches post check.
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckSplitValue("path1", &dict, &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());

  // Store hash for |modified_dict|.
  pref_hash_store.StoreSplitHash("path1", &modified_dict);

  // Verify |modified_dict| is now the one that verifies correctly.
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckSplitValue("path1", &modified_dict,
                                            &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());

  // Verify old dict no longer matches.
  EXPECT_EQ(PrefHashStore::CHANGED,
            pref_hash_store.CheckSplitValue("path1", &dict, &invalid_keys));
  std::vector<std::string> expected_invalid_keys2;
  expected_invalid_keys2.push_back("a");
  expected_invalid_keys2.push_back("o");
  EXPECT_EQ(expected_invalid_keys2, invalid_keys);
  invalid_keys.clear();

  // |pref_hash_store2| should trust its initial hashes dictionary and thus
  // trust new unknown values.
  PrefHashStoreImpl pref_hash_store2(
      "store_id", std::string(32, 0), "device_id", &local_state);
  EXPECT_EQ(PrefHashStore::TRUSTED_UNKNOWN_VALUE,
            pref_hash_store2.CheckSplitValue("new_path", &dict, &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());

  {
    // Manually corrupt the hash of hashes for "store_id".
    DictionaryPrefUpdate update(&local_state, prefs::kProfilePreferenceHashes);
    base::DictionaryValue* hash_of_hashes_dict = NULL;
    ASSERT_TRUE(update->GetDictionaryWithoutPathExpansion(
        internals::kHashOfHashesPref, &hash_of_hashes_dict));
    hash_of_hashes_dict->SetString("store_id", std::string(64, 'A'));
    // This shouldn't have increased the number of existing hash of hashes.
    ASSERT_EQ(1U, hash_of_hashes_dict->size());
  }

  // |pref_hash_store3| should no longer trust its initial hashes dictionary and
  // thus shouldn't trust unknown values.
  PrefHashStoreImpl pref_hash_store3(
      "store_id", std::string(32, 0), "device_id", &local_state);
  EXPECT_EQ(PrefHashStore::UNTRUSTED_UNKNOWN_VALUE,
            pref_hash_store3.CheckSplitValue("new_path", &dict, &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());
}

TEST(PrefHashStoreImplTest, EmptyAndNULLSplitDict) {
  base::DictionaryValue empty_dict;

  TestingPrefServiceSimple local_state;
  PrefHashStoreImpl::RegisterPrefs(local_state.registry());

  PrefHashStoreImpl pref_hash_store(
      "store_id", std::string(32, 0), "device_id", &local_state);

  // Store hashes for a random dict to be overwritten below.
  base::DictionaryValue initial_dict;
  initial_dict.Set("a", new base::StringValue("foo"));
  pref_hash_store.StoreSplitHash("path1", &initial_dict);

  std::vector<std::string> invalid_keys;

  // Verify stored empty dictionary matches NULL and empty dictionary back.
  pref_hash_store.StoreSplitHash("path1", &empty_dict);
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckSplitValue("path1", NULL, &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckSplitValue("path1", &empty_dict,
                                            &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());

  // Same when storing NULL directly.
  pref_hash_store.StoreSplitHash("path1", NULL);
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckSplitValue("path1", NULL, &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckSplitValue("path1", &empty_dict,
                                            &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());

  // |pref_hash_store2| should trust its initial hashes dictionary (and thus
  // trust new unknown values) even though the last action done on local_state
  // was to clear the hashes for path1 by setting its value to NULL (this is a
  // regression test ensuring that the internal action of clearing some hashes
  // does update the stored hash of hashes).
  PrefHashStoreImpl pref_hash_store2(
      "store_id", std::string(32, 0), "device_id", &local_state);

  base::DictionaryValue tested_dict;
  tested_dict.Set("a", new base::StringValue("foo"));
  tested_dict.Set("b", new base::StringValue("bar"));
  EXPECT_EQ(PrefHashStore::TRUSTED_UNKNOWN_VALUE,
            pref_hash_store2.CheckSplitValue("new_path", &tested_dict,
                                             &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());
}

// Test that the PrefHashStore returns TRUSTED_UNKNOWN_VALUE when checking for
// a split preference even if there is an existing atomic preference's hash
// stored. There is no point providing a migration path for preferences
// switching strategies after their initial release as split preferences are
// turned into split preferences specifically because the atomic hash isn't
// considered useful.
TEST(PrefHashStoreImplTest, TrustedUnknownSplitValueFromExistingAtomic) {
  base::StringValue string("string1");

  base::DictionaryValue dict;
  dict.Set("a", new base::StringValue("foo"));
  dict.Set("d", new base::StringValue("bad"));
  dict.Set("b", new base::StringValue("bar"));
  dict.Set("c", new base::StringValue("baz"));

  TestingPrefServiceSimple local_state;
  PrefHashStoreImpl::RegisterPrefs(local_state.registry());

  PrefHashStoreImpl pref_hash_store(
      "store_id", std::string(32, 0), "device_id", &local_state);

  pref_hash_store.StoreHash("path1", &string);
  EXPECT_EQ(PrefHashStore::UNCHANGED,
            pref_hash_store.CheckValue("path1", &string));

  // Load a new |pref_hash_store2| in which the hashes dictionary is trusted.
  PrefHashStoreImpl pref_hash_store2(
      "store_id", std::string(32, 0), "device_id", &local_state);
  std::vector<std::string> invalid_keys;
  EXPECT_EQ(PrefHashStore::TRUSTED_UNKNOWN_VALUE,
            pref_hash_store2.CheckSplitValue("path1", &dict, &invalid_keys));
  EXPECT_TRUE(invalid_keys.empty());
}
