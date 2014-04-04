// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_store_impl.h"

#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_hash_store_impl.h"
#include "chrome/browser/prefs/pref_hash_store_transaction.h"
#include "chrome/browser/prefs/tracked/hash_store_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockHashStoreContents : public HashStoreContents {
 public:
  // Keep the data separate from the API implementation so that it can be owned
  // by the test and reused. The API implementation is owned by the
  // PrefHashStoreImpl.
  struct Data {
    Data() : commit_performed(false) {}

    // Returns the current value of |commit_performed| and resets it to false
    // immediately after.
    bool GetCommitPerformedAndReset() {
      bool current_commit_performed = commit_performed;
      commit_performed = false;
      return current_commit_performed;
    }

    scoped_ptr<base::DictionaryValue> contents;
    std::string super_mac;
    scoped_ptr<int> version;
    bool commit_performed;
  };

  explicit MockHashStoreContents(Data* data) : data_(data) {}

  // HashStoreContents implementation
  virtual std::string hash_store_id() const OVERRIDE { return "store_id"; }

  virtual void Reset() OVERRIDE {
    data_->contents.reset();
    data_->super_mac = "";
    data_->version.reset();
  }

  virtual bool IsInitialized() const OVERRIDE { return data_->contents; }

  virtual bool GetVersion(int* version) const OVERRIDE {
    if (data_->version)
      *version = *data_->version;
    return data_->version;
  }

  virtual void SetVersion(int version) OVERRIDE {
    data_->version.reset(new int(version));
  }

  virtual const base::DictionaryValue* GetContents() const OVERRIDE {
    return data_->contents.get();
  }

  virtual scoped_ptr<MutableDictionary> GetMutableContents() OVERRIDE {
    return scoped_ptr<MutableDictionary>(new MockMutableDictionary(data_));
  }

  virtual std::string GetSuperMac() const OVERRIDE { return data_->super_mac; }

  virtual void SetSuperMac(const std::string& super_mac) OVERRIDE {
    data_->super_mac = super_mac;
  }

  virtual void CommitPendingWrite() OVERRIDE {
    EXPECT_FALSE(data_->commit_performed);
    data_->commit_performed = true;
  }

 private:
  class MockMutableDictionary : public MutableDictionary {
   public:
    explicit MockMutableDictionary(Data* data) : data_(data) {}

    // MutableDictionary implementation
    virtual base::DictionaryValue* operator->() OVERRIDE {
      if (!data_->contents)
        data_->contents.reset(new base::DictionaryValue);
      return data_->contents.get();
    }

   private:
    Data* data_;
    DISALLOW_COPY_AND_ASSIGN(MockMutableDictionary);
  };

  Data* data_;

  DISALLOW_COPY_AND_ASSIGN(MockHashStoreContents);
};

class PrefHashStoreImplTest : public testing::Test {
 protected:
  scoped_ptr<HashStoreContents> CreateHashStoreContents() {
    return scoped_ptr<HashStoreContents>(
        new MockHashStoreContents(&hash_store_data_));
  }

  MockHashStoreContents::Data hash_store_data_;
};

TEST_F(PrefHashStoreImplTest, AtomicHashStoreAndCheck) {
  base::StringValue string_1("string1");
  base::StringValue string_2("string2");

  {
    // 32 NULL bytes is the seed that was used to generate the legacy hash.
    PrefHashStoreImpl pref_hash_store(
        std::string(32, 0), "device_id", CreateHashStoreContents());
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

    // Manually shove in a legacy hash.
    (*CreateHashStoreContents()->GetMutableContents())->SetString(
        "path1",
        "C503FB7C65EEFD5C07185F616A0AA67923C069909933F362022B1F187E73E9A2");

    EXPECT_EQ(PrefHashStoreTransaction::WEAK_LEGACY,
              transaction->CheckValue("path1", &dict));
    transaction->StoreHash("path1", &dict);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &dict));

    // Test that the |pref_hash_store| flushes its changes on request post
    // transaction.
    transaction.reset();
    pref_hash_store.CommitPendingWrite();
    EXPECT_TRUE(hash_store_data_.GetCommitPerformedAndReset());
  }

  {
    // |pref_hash_store2| should trust its initial hashes dictionary and thus
    // trust new unknown values.
    PrefHashStoreImpl pref_hash_store2(
        std::string(32, 0), "device_id", CreateHashStoreContents());
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction());
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_2));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", NULL));

    // Test that |pref_hash_store2| doesn't flush its contents to disk when it
    // didn't change.
    transaction.reset();
    pref_hash_store2.CommitPendingWrite();
    EXPECT_FALSE(hash_store_data_.GetCommitPerformedAndReset());
  }

  // Manually corrupt the super MAC.
  hash_store_data_.super_mac = std::string(64, 'A');

  {
    // |pref_hash_store3| should no longer trust its initial hashes dictionary
    // and thus shouldn't trust non-NULL unknown values.
    PrefHashStoreImpl pref_hash_store3(
        std::string(32, 0), "device_id", CreateHashStoreContents());
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store3.BeginTransaction());
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_1));
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", &string_2));
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckValue("new_path", NULL));

    // Test that |pref_hash_store3| doesn't flush its contents to disk when it
    // didn't change.
    transaction.reset();
    pref_hash_store3.CommitPendingWrite();
    EXPECT_FALSE(hash_store_data_.GetCommitPerformedAndReset());
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
        std::string(32, 0), "device_id", CreateHashStoreContents());
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
    EXPECT_EQ(
        PrefHashStoreTransaction::CLEARED,
        transaction->CheckSplitValue("path1", &empty_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify changes are properly detected.
    EXPECT_EQ(
        PrefHashStoreTransaction::CHANGED,
        transaction->CheckSplitValue("path1", &modified_dict, &invalid_keys));
    std::vector<std::string> expected_invalid_keys1;
    expected_invalid_keys1.push_back("a");
    expected_invalid_keys1.push_back("c");
    expected_invalid_keys1.push_back("o");
    EXPECT_EQ(expected_invalid_keys1, invalid_keys);
    invalid_keys.clear();

    // Verify |dict| still matches post check.
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Store hash for |modified_dict|.
    transaction->StoreSplitHash("path1", &modified_dict);

    // Verify |modified_dict| is now the one that verifies correctly.
    EXPECT_EQ(
        PrefHashStoreTransaction::UNCHANGED,
        transaction->CheckSplitValue("path1", &modified_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Verify old dict no longer matches.
    EXPECT_EQ(PrefHashStoreTransaction::CHANGED,
              transaction->CheckSplitValue("path1", &dict, &invalid_keys));
    std::vector<std::string> expected_invalid_keys2;
    expected_invalid_keys2.push_back("a");
    expected_invalid_keys2.push_back("o");
    expected_invalid_keys2.push_back("c");
    EXPECT_EQ(expected_invalid_keys2, invalid_keys);
    invalid_keys.clear();

    // Test that the |pref_hash_store| flushes its changes on request.
    transaction.reset();
    pref_hash_store.CommitPendingWrite();
    EXPECT_TRUE(hash_store_data_.GetCommitPerformedAndReset());
  }

  {
    // |pref_hash_store2| should trust its initial hashes dictionary and thus
    // trust new unknown values.
    PrefHashStoreImpl pref_hash_store2(
        std::string(32, 0), "device_id", CreateHashStoreContents());
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction());
    EXPECT_EQ(PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Test that |pref_hash_store2| doesn't flush its contents to disk when it
    // didn't change.
    transaction.reset();
    pref_hash_store2.CommitPendingWrite();
    EXPECT_FALSE(hash_store_data_.GetCommitPerformedAndReset());
  }

  // Manually corrupt the super MAC.
  hash_store_data_.super_mac = std::string(64, 'A');

  {
    // |pref_hash_store3| should no longer trust its initial hashes dictionary
    // and thus shouldn't trust unknown values.
    PrefHashStoreImpl pref_hash_store3(
        std::string(32, 0), "device_id", CreateHashStoreContents());
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store3.BeginTransaction());
    EXPECT_EQ(PrefHashStoreTransaction::UNTRUSTED_UNKNOWN_VALUE,
              transaction->CheckSplitValue("new_path", &dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Test that |pref_hash_store3| doesn't flush its contents to disk when it
    // didn't change.
    transaction.reset();
    pref_hash_store3.CommitPendingWrite();
    EXPECT_FALSE(hash_store_data_.GetCommitPerformedAndReset());
  }
}

TEST_F(PrefHashStoreImplTest, EmptyAndNULLSplitDict) {
  base::DictionaryValue empty_dict;

  std::vector<std::string> invalid_keys;

  {
    PrefHashStoreImpl pref_hash_store(
        std::string(32, 0), "device_id", CreateHashStoreContents());
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
    EXPECT_EQ(
        PrefHashStoreTransaction::UNCHANGED,
        transaction->CheckSplitValue("path1", &empty_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());

    // Same when storing NULL directly.
    transaction->StoreSplitHash("path1", NULL);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckSplitValue("path1", NULL, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
    EXPECT_EQ(
        PrefHashStoreTransaction::UNCHANGED,
        transaction->CheckSplitValue("path1", &empty_dict, &invalid_keys));
    EXPECT_TRUE(invalid_keys.empty());
  }

  {
    // |pref_hash_store2| should trust its initial hashes dictionary (and thus
    // trust new unknown values) even though the last action done was to clear
    // the hashes for path1 by setting its value to NULL (this is a regression
    // test ensuring that the internal action of clearing some hashes does
    // update the stored hash of hashes).
    PrefHashStoreImpl pref_hash_store2(
        std::string(32, 0), "device_id", CreateHashStoreContents());
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store2.BeginTransaction());

    base::DictionaryValue tested_dict;
    tested_dict.Set("a", new base::StringValue("foo"));
    tested_dict.Set("b", new base::StringValue("bar"));
    EXPECT_EQ(
        PrefHashStoreTransaction::TRUSTED_UNKNOWN_VALUE,
        transaction->CheckSplitValue("new_path", &tested_dict, &invalid_keys));
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
        std::string(32, 0), "device_id", CreateHashStoreContents());
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction());

    transaction->StoreHash("path1", &string);
    EXPECT_EQ(PrefHashStoreTransaction::UNCHANGED,
              transaction->CheckValue("path1", &string));
  }

  {
    // Load a new |pref_hash_store2| in which the hashes dictionary is trusted.
    PrefHashStoreImpl pref_hash_store2(
        std::string(32, 0), "device_id", CreateHashStoreContents());
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
        std::string(32, 0), "device_id", CreateHashStoreContents());

    // VERSION_UNINITIALIZED when no hashes are stored.
    EXPECT_EQ(PrefHashStoreImpl::VERSION_UNINITIALIZED,
              pref_hash_store.GetCurrentVersion());

    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction());
    base::StringValue string_value("foo");
    transaction->StoreHash("path1", &string_value);

    // Test that |pref_hash_store| flushes its content to disk when it
    // initializes its version.
    transaction.reset();
    pref_hash_store.CommitPendingWrite();
    EXPECT_TRUE(hash_store_data_.GetCommitPerformedAndReset());
  }
  {
    PrefHashStoreImpl pref_hash_store(
        std::string(32, 0), "device_id", CreateHashStoreContents());

    // VERSION_LATEST after storing a hash.
    EXPECT_EQ(PrefHashStoreImpl::VERSION_LATEST,
              pref_hash_store.GetCurrentVersion());

    // Test that |pref_hash_store| doesn't flush its contents to disk when it
    // didn't change.
    pref_hash_store.CommitPendingWrite();
    EXPECT_FALSE(hash_store_data_.GetCommitPerformedAndReset());
  }

  // Manually clear the version number.
  hash_store_data_.version.reset();

  {
    PrefHashStoreImpl pref_hash_store(
        std::string(32, 0), "device_id", CreateHashStoreContents());

    // VERSION_PRE_MIGRATION when no version is stored.
    EXPECT_EQ(PrefHashStoreImpl::VERSION_PRE_MIGRATION,
              pref_hash_store.GetCurrentVersion());

    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction());

    // Test that |pref_hash_store| flushes its content to disk when it
    // re-initializes its version.
    transaction.reset();
    pref_hash_store.CommitPendingWrite();
    EXPECT_TRUE(hash_store_data_.GetCommitPerformedAndReset());
  }
  {
    PrefHashStoreImpl pref_hash_store(
        std::string(32, 0), "device_id", CreateHashStoreContents());

    // Back to VERSION_LATEST after performing a transaction from
    // VERSION_PRE_MIGRATION (the presence of an existing hash should be
    // sufficient, no need for the transaction itself to perform any work).
    EXPECT_EQ(PrefHashStoreImpl::VERSION_LATEST,
              pref_hash_store.GetCurrentVersion());

    // Test that |pref_hash_store| doesn't flush its contents to disk when it
    // didn't change (i.e., its version was already up-to-date and the only
    // transaction performed was empty).
    scoped_ptr<PrefHashStoreTransaction> transaction(
        pref_hash_store.BeginTransaction());
    transaction.reset();
    pref_hash_store.CommitPendingWrite();
    EXPECT_FALSE(hash_store_data_.GetCommitPerformedAndReset());
  }
}
