// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_store_impl.h"

#include <string>

#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store_impl.h"
#include "chrome/browser/prefs/pref_hash_store_transaction.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefHashStoreImplTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    PrefHashStoreImpl::RegisterPrefs(local_state_.registry());
  }

 protected:
  TestingPrefServiceSimple local_state_;
};

TEST_F(PrefHashStoreImplTest, AtomicHashStoreAndCheck) {
  base::StringValue string_1("string1");
  base::StringValue string_2("string2");

  {
    // 32 NULL bytes is the seed that was used to generate the legacy hash.
    PrefHashStoreImpl pref_hash_store(
        "store_id", std::string(32, 0), "device_id", &local_state_);
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction());

    // Only NULL should be trusted in the absence of a hash.
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("path1", NULL));

    transaction->StoreHash("path1", &string_1);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::CLEARED,
              transaction->CheckValue("path1", NULL));
    transaction->StoreHash("path1", NULL);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", NULL));
    EXPECT_EQ(PrefHashStoreTransaction::CHANGED,
              transaction->CheckValue("path1", &string_2));

    base::DictionaryValue dict;
    dict.Set("a", new base::StringValue("foo"));
    dict.Set("d", new base::StringValue("bad"));
    dict.Set("b", new base::StringValue("bar"));
    dict.Set("c", new base::StringValue("baz"));

    {
      // Manually shove in a legacy hash.
      DictionaryPrefUpdate update(&local_state_,
                                  prefs::kProfilePreferenceHashes);
      base::DictionaryValue* child_dictionary = NULL;
      ASSERT_TRUE(update->GetDictionary("store_id", &child_dictionary));
      child_dictionary->SetString(
          "path1",
          "C503FB7C65EEFD5C07185F616A0AA67923C069909933F362022B1F187E73E9A2");
    }
    EXPECT_EQ(PrefHashStoreTransaction::WEAK_LEGACY,
              transaction->CheckValue("path1", &dict));
    transaction->StoreHash("path1", &dict);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &dict));
  }

  {
    // |pref_hash_store2| should trust its initial hashes dictionary and thus
    // trust new unknown values.
    PrefHashStoreImpl pref_hash_store2(
        "store_id", std::string(32, 0), "device_id", &local_state_);
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction());
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_2));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", NULL));
  }

  {
    // Manually corrupt the hash of hashes for "store_id".
    DictionaryPrefUpdate update(&local_state_, prefs::kProfilePreferenceHashes);
    base::DictionaryValue* hash_of_hashes_dict = NULL;
    ASSERT_TRUE(update->GetDictionaryWithoutPathExpansion(
        internals::kHashOfHashesDict, &hash_of_hashes_dict));
    hash_of_hashes_dict->SetString("store_id", std::string(64, 'A'));
    // This shouldn't have increased the number of existing hash of hashes.
    ASSERT_EQ(1U, hash_of_hashes_dict->size());
  }

  {
    // |pref_hash_store3| should no longer trust its initial hashes dictionary
    // and thus shouldn't trust non-NULL unknown values.
    PrefHashStoreImpl pref_hash_store3(
        "store_id", std::string(32, 0), "device_id", &local_state_);
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store3.BeginTransaction());
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_2));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", NULL));
  }
}

TEST_F(PrefHashStoreImplTest, SplitHashStoreAndCheck) {
  base::DictionaryValue dict;
  dict.Set("a", new base::StringValue("to be replaced"));
  dict.Set("b", new base::StringValue("same"));
  dict.Set("o", new base::StringValue("old"));

  base::DictionaryValue modified_dict;
  modified_dict.Set("a", new base::StringValue("replaced"));
  modified_dict.Set("b", new base::StringValue("same"));
  modified_dict.Set("c", new base::StringValue("new"));

  base::DictionaryValue empty_dict;

  std::vector<std::string> invalid_keys;

  {
    PrefHashStoreImpl pref_hash_store(
        "store_id", std::string(32, 0), "device_id", &local_state_);
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction());

    // No hashes stored yet and hashes dictionary is empty (and thus not
    // trusted).
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    transaction->StoreSplitHash("path1", &dict);

    // Verify match post storage.
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify new path is still unknown.
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("path2", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify NULL or empty dicts are declared as having been cleared.
    EXPECT_EQ(PrefHashStoreTransaction::CLEARED,
              transaction->CheckSplitValue("path1", NULL, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(PrefHashStoreTransaction::CLEARED,
              transaction->CheckSplitValue("path1", &empty_dict,
                                              &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify changes are properly detected.
    EXPECT_EQ(PrefHashStoreTransaction::CHANGED,
              transaction->CheckSplitValue("path1", &modified_dict,
                                              &invalid_keys));
    std::vector<std::string> expected_invalid_keys1;
    expected_invalid_keys1.push_back("a");
    expected_invalid_keys1.push_back("c");
    EXPECT_EQ(expected_invalid_keys1, invalid_keys);
    invalid_keys.clear();

    // Verify |dict| still matches post check.
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Store hash for |modified_dict|.
    transaction->StoreSplitHash("path1", &modified_dict);

    // Verify |modified_dict| is now the one that verifies correctly.
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", &modified_dict,
                                              &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify old dict no longer matches.
    EXPECT_EQ(PrefHashStoreTransaction::CHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    std::vector<std::string> expected_invalid_keys2;
    expected_invalid_keys2.push_back("a");
    expected_invalid_keys2.push_back("o");
    EXPECT_EQ(expected_invalid_keys2, invalid_keys);
    invalid_keys.clear();
  }

  {
    // |pref_hash_store2| should trust its initial hashes dictionary and thus
    // trust new unknown values.
    PrefHashStoreImpl pref_hash_store2(
        "store_id", std::string(32, 0), "device_id", &local_state_);
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction());
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }

  {
    // Manually corrupt the hash of hashes for "store_id".
    DictionaryPrefUpdate update(&local_state_, prefs::kProfilePreferenceHashes);
    base::DictionaryValue* hash_of_hashes_dict = NULL;
    ASSERT_TRUE(update->GetDictionaryWithoutPathExpansion(
        internals::kHashOfHashesDict, &hash_of_hashes_dict));
    hash_of_hashes_dict->SetString("store_id", std::string(64, 'A'));
    // This shouldn't have increased the number of existing hash of hashes.
    ASSERT_EQ(1U, hash_of_hashes_dict->size());
  }

  {
    // |pref_hash_store3| should no longer trust its initial hashes dictionary
    // and thus shouldn't trust unknown values.
    PrefHashStoreImpl pref_hash_store3(
        "store_id", std::string(32, 0), "device_id", &local_state_);
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store3.BeginTransaction());
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

TEST_F(PrefHashStoreImplTest, EmptyAndNULLSplitDict) {
  base::DictionaryValue empty_dict;

  std::vector<std::string> invalid_keys;

  {
    PrefHashStoreImpl pref_hash_store(
        "store_id", std::string(32, 0), "device_id", &local_state_);
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction());

    // Store hashes for a random dict to be overwritten below.
    base::DictionaryValue initial_dict;
    initial_dict.Set("a", new base::StringValue("foo"));
    transaction->StoreSplitHash("path1", &initial_dict);

    // Verify stored empty dictionary matches NULL and empty dictionary back.
    transaction->StoreSplitHash("path1", &empty_dict);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", NULL, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", &empty_dict,
                                              &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Same when storing NULL directly.
    transaction->StoreSplitHash("path1", NULL);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", NULL, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", &empty_dict,
                                              &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }

  {
    // |pref_hash_store2| should trust its initial hashes dictionary (and thus
    // trust new unknown values) even though the last action done on local_state
    // was to clear the hashes for path1 by setting its value to NULL (this is a
    // regression test ensuring that the internal action of clearing some hashes
    // does update the stored hash of hashes).
    PrefHashStoreImpl pref_hash_store2(
        "store_id", std::string(32, 0), "device_id", &local_state_);
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction());

    base::DictionaryValue tested_dict;
    tested_dict.Set("a", new base::StringValue("foo"));
    tested_dict.Set("b", new base::StringValue("bar"));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &tested_dict,
                                           &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

// Test that the PrefHashStore returns TRUSTED_UNKNOWN_VALUE when checking for
// a split preference even if there is an existing atomic preference's hash
// stored. There is no point providing a migration path for preferences
// switching strategies after their initial release as split preferences are
// turned into split preferences specifically because the atomic hash isn't
// considered useful.
TEST_F(PrefHashStoreImplTest, TrustedUnknownSplitValueFromExistingAtomic) {
  base::StringValue string("string1");

  base::DictionaryValue dict;
  dict.Set("a", new base::StringValue("foo"));
  dict.Set("d", new base::StringValue("bad"));
  dict.Set("b", new base::StringValue("bar"));
  dict.Set("c", new base::StringValue("baz"));

  {
    PrefHashStoreImpl pref_hash_store(
        "store_id", std::string(32, 0), "device_id", &local_state_);
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction());

    transaction->StoreHash("path1", &string);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string));
  }

  {
    // Load a new |pref_hash_store2| in which the hashes dictionary is trusted.
    PrefHashStoreImpl pref_hash_store2(
        "store_id", std::string(32, 0), "device_id", &local_state_);
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction());
    std::vector<std::string> invalid_keys;
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }
}

TEST_F(PrefHashStoreImplTest, GetCurrentVersion) {
  COMPILE_ASSERT(PrefHashStoreImpl::VERSION_LATEST == 2,
                 new_versions_should_be_tested_here);
  {
    PrefHashStoreImpl pref_hash_store(
        "store_id", std::string(32, 0), "device_id", &local_state_);

    // VERSION_UNINITIALIZED when no hashes are stored.
    EXPECT_EQ(PrefHashStoreImpl::VERSION_UNINITIALIZED,
              pref_hash_store.GetCurrentVersion());

    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction());
    base::StringValue string_value("foo");
    transaction->StoreHash("path1", &string_value);
  }
  {
    PrefHashStoreImpl pref_hash_store(
        "store_id", std::string(32, 0), "device_id", &local_state_);

    // VERSION_LATEST after storing a hash.
    EXPECT_EQ(PrefHashStoreImpl::VERSION_LATEST,
              pref_hash_store.GetCurrentVersion());
  }
  {
    // Manually clear the version number.
    DictionaryPrefUpdate update(&local_state_, prefs::kProfilePreferenceHashes);
    base::DictionaryValue* store_versions_dict = NULL;
    ASSERT_TRUE(update->GetDictionaryWithoutPathExpansion(
        internals::kStoreVersionsDict, &store_versions_dict));
    scoped_ptr<base::Value> stored_value;
    store_versions_dict->Remove("store_id", &stored_value);
    int stored_version;
    EXPECT_TRUE(stored_value->GetAsInteger(&stored_version));
    EXPECT_EQ(PrefHashStoreImpl::VERSION_LATEST, stored_version);
    // There should be no versions left in the versions dictionary.
    EXPECT_TRUE(store_versions_dict->empty());
  }
  {
    PrefHashStoreImpl pref_hash_store(
        "store_id", std::string(32, 0), "device_id", &local_state_);

    // VERSION_PRE_MIGRATION when no version is present for "store_id".
    EXPECT_EQ(PrefHashStoreImpl::VERSION_PRE_MIGRATION,
              pref_hash_store.GetCurrentVersion());

    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction());
  }
  {
    PrefHashStoreImpl pref_hash_store(
        "store_id", std::string(32, 0), "device_id", &local_state_);

    // Back to VERSION_LATEST after performing a transaction from
    // VERSION_PRE_MIGRATION (the presence of an existing hash should be
    // sufficient, no need for the transaction itself to perform any work).
    EXPECT_EQ(PrefHashStoreImpl::VERSION_LATEST,
              pref_hash_store.GetCurrentVersion());
  }
}

TEST_F(PrefHashStoreImplTest, ResetAllPrefHashStores) {
  {
    PrefHashStoreImpl pref_hash_store_1(
        "store_id_1", std::string(32, 0), "device_id", &local_state_);
    PrefHashStoreImpl pref_hash_store_2(
        "store_id_2", std::string(32, 0), "device_id", &local_state_);

    // VERSION_UNINITIALIZED when no hashes are stored.
    EXPECT_EQ(PrefHashStoreImpl::VERSION_UNINITIALIZED,
              pref_hash_store_1.GetCurrentVersion());
    EXPECT_EQ(PrefHashStoreImpl::VERSION_UNINITIALIZED,
              pref_hash_store_2.GetCurrentVersion());

    scoped_ptr<PrefHashStoreTransaction> transaction_1(
        pref_hash_store_1.BeginTransaction());
    transaction_1->StoreHash("path1", NULL);

    scoped_ptr<PrefHashStoreTransaction> transaction_2(
        pref_hash_store_2.BeginTransaction());
    transaction_2->StoreHash("path2", NULL);
  }
  {
    PrefHashStoreImpl pref_hash_store_1(
        "store_id_1", std::string(32, 0), "device_id", &local_state_);
    PrefHashStoreImpl pref_hash_store_2(
        "store_id_2", std::string(32, 0), "device_id", &local_state_);

    // VERSION_LATEST after storing a hash.
    EXPECT_EQ(PrefHashStoreImpl::VERSION_LATEST,
              pref_hash_store_1.GetCurrentVersion());
    EXPECT_EQ(PrefHashStoreImpl::VERSION_LATEST,
              pref_hash_store_2.GetCurrentVersion());
  }

  PrefHashStoreImpl::ResetAllPrefHashStores(&local_state_);

  {
    PrefHashStoreImpl pref_hash_store_1(
        "store_id_1", std::string(32, 0), "device_id", &local_state_);
    PrefHashStoreImpl pref_hash_store_2(
        "store_id_2", std::string(32, 0), "device_id", &local_state_);

    // VERSION_UNINITIALIZED after ResetAllPrefHashStores.
    EXPECT_EQ(PrefHashStoreImpl::VERSION_UNINITIALIZED,
              pref_hash_store_1.GetCurrentVersion());
    EXPECT_EQ(PrefHashStoreImpl::VERSION_UNINITIALIZED,
              pref_hash_store_2.GetCurrentVersion());
  }
}
