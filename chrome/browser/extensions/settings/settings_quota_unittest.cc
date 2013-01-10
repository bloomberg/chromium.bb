// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/settings/settings_backend.h"
#include "chrome/browser/extensions/settings/settings_storage_quota_enforcer.h"
#include "chrome/browser/value_store/testing_value_store.h"

namespace extensions {

// To save typing ValueStore::DEFAULTS/IGNORE_QUOTA everywhere.
const ValueStore::WriteOptions DEFAULTS = ValueStore::DEFAULTS;
const ValueStore::WriteOptions IGNORE_QUOTA =
    ValueStore::IGNORE_QUOTA;

class ExtensionSettingsQuotaTest : public testing::Test {
 public:
  ExtensionSettingsQuotaTest()
      : byte_value_1_(Value::CreateIntegerValue(1)),
        byte_value_16_(Value::CreateStringValue("sixteen bytes.")),
        byte_value_256_(new ListValue()),
        delegate_(new TestingValueStore()) {
    for (int i = 1; i < 89; ++i) {
      byte_value_256_->Append(Value::CreateIntegerValue(i));
    }
    ValidateByteValues();
  }

  void ValidateByteValues() {
    std::string validate_sizes;
    base::JSONWriter::Write(byte_value_1_.get(), &validate_sizes);
    ASSERT_EQ(1u, validate_sizes.size());
    base::JSONWriter::Write(byte_value_16_.get(), &validate_sizes);
    ASSERT_EQ(16u, validate_sizes.size());
    base::JSONWriter::Write(byte_value_256_.get(), &validate_sizes);
    ASSERT_EQ(256u, validate_sizes.size());
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(storage_.get() != NULL);
  }

 protected:
  // Creates |storage_|.  Must only be called once.
  void CreateStorage(
      size_t quota_bytes, size_t quota_bytes_per_item, size_t max_items) {
    ASSERT_TRUE(storage_.get() == NULL);
    SettingsStorageQuotaEnforcer::Limits limits =
        { quota_bytes, quota_bytes_per_item, max_items };
    storage_.reset(new SettingsStorageQuotaEnforcer(limits, delegate_));
  }

  // Returns whether the settings in |storage_| and |delegate_| are the same as
  // |settings|.
  bool SettingsEqual(const DictionaryValue& settings) {
    return settings.Equals(storage_->Get()->settings().get()) &&
           settings.Equals(delegate_->Get()->settings().get());
  }

  // Values with different serialized sizes.
  scoped_ptr<Value> byte_value_1_;
  scoped_ptr<Value> byte_value_16_;
  scoped_ptr<ListValue> byte_value_256_;

  // Quota enforcing storage area being tested.
  scoped_ptr<SettingsStorageQuotaEnforcer> storage_;

  // In-memory storage area being delegated to.  Always owned by |storage_|.
  TestingValueStore* delegate_;
};

TEST_F(ExtensionSettingsQuotaTest, ZeroQuotaBytes) {
  DictionaryValue empty;
  CreateStorage(0, UINT_MAX, UINT_MAX);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Remove("a")->HasError());
  EXPECT_FALSE(storage_->Remove("b")->HasError());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(ExtensionSettingsQuotaTest, KeySizeTakenIntoAccount) {
  DictionaryValue empty;
  CreateStorage(8u, UINT_MAX, UINT_MAX);
  EXPECT_TRUE(
      storage_->Set(DEFAULTS, "Really long key", *byte_value_1_)->HasError());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(ExtensionSettingsQuotaTest, SmallByteQuota) {
  DictionaryValue settings;
  CreateStorage(8u, UINT_MAX, UINT_MAX);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  settings.Set("a", byte_value_1_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", *byte_value_16_)->HasError());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", *byte_value_256_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, MediumByteQuota) {
  DictionaryValue settings;
  CreateStorage(40, UINT_MAX, UINT_MAX);

  DictionaryValue to_set;
  to_set.Set("a", byte_value_1_->DeepCopy());
  to_set.Set("b", byte_value_16_->DeepCopy());
  EXPECT_FALSE(storage_->Set(DEFAULTS, to_set)->HasError());
  settings.Set("a", byte_value_1_->DeepCopy());
  settings.Set("b", byte_value_16_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able to set value to other under-quota value.
  to_set.Set("a", byte_value_16_->DeepCopy());
  EXPECT_FALSE(storage_->Set(DEFAULTS, to_set)->HasError());
  settings.Set("a", byte_value_16_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", *byte_value_256_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, ZeroMaxKeys) {
  DictionaryValue empty;
  CreateStorage(UINT_MAX, UINT_MAX, 0);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Remove("a")->HasError());
  EXPECT_FALSE(storage_->Remove("b")->HasError());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(ExtensionSettingsQuotaTest, SmallMaxKeys) {
  DictionaryValue settings;
  CreateStorage(UINT_MAX, UINT_MAX, 1);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  settings.Set("a", byte_value_1_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able to set existing key to other value without going over quota.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", *byte_value_16_)->HasError());
  settings.Set("a", byte_value_16_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", *byte_value_16_)->HasError());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", *byte_value_256_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, MediumMaxKeys) {
  DictionaryValue settings;
  CreateStorage(UINT_MAX, UINT_MAX, 2);

  DictionaryValue to_set;
  to_set.Set("a", byte_value_1_->DeepCopy());
  to_set.Set("b", byte_value_16_->DeepCopy());
  EXPECT_FALSE(storage_->Set(DEFAULTS, to_set)->HasError());
  settings.Set("a", byte_value_1_->DeepCopy());
  settings.Set("b", byte_value_16_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able to set existing keys to other values without going over
  // quota.
  to_set.Set("a", byte_value_16_->DeepCopy());
  EXPECT_FALSE(storage_->Set(DEFAULTS, to_set)->HasError());
  settings.Set("a", byte_value_16_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", *byte_value_256_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, RemovingExistingSettings) {
  DictionaryValue settings;
  CreateStorage(266, UINT_MAX, 2);

  storage_->Set(DEFAULTS, "b", *byte_value_16_);
  settings.Set("b", byte_value_16_->DeepCopy());
  // Not enough quota.
  storage_->Set(DEFAULTS, "c", *byte_value_256_);
  EXPECT_TRUE(SettingsEqual(settings));

  // Try again with "b" removed, enough quota.
  EXPECT_FALSE(storage_->Remove("b")->HasError());
  settings.Remove("b", NULL);
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", *byte_value_256_)->HasError());
  settings.Set("c", byte_value_256_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  // Enough byte quota but max keys not high enough.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  settings.Set("a", byte_value_1_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", *byte_value_1_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));

  // Back under max keys.
  EXPECT_FALSE(storage_->Remove("a")->HasError());
  settings.Remove("a", NULL);
  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", *byte_value_1_)->HasError());
  settings.Set("b", byte_value_1_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, RemovingNonexistentSettings) {
  DictionaryValue settings;
  CreateStorage(36, UINT_MAX, 3);

  // Max out bytes.
  DictionaryValue to_set;
  to_set.Set("b1", byte_value_16_->DeepCopy());
  to_set.Set("b2", byte_value_16_->DeepCopy());
  storage_->Set(DEFAULTS, to_set);
  settings.Set("b1", byte_value_16_->DeepCopy());
  settings.Set("b2", byte_value_16_->DeepCopy());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));

  // Remove some settings that don't exist.
  std::vector<std::string> to_remove;
  to_remove.push_back("a1");
  to_remove.push_back("a2");
  EXPECT_FALSE(storage_->Remove(to_remove)->HasError());
  EXPECT_FALSE(storage_->Remove("b")->HasError());
  EXPECT_TRUE(SettingsEqual(settings));

  // Still no quota.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));

  // Max out key count.
  to_set.Clear();
  to_set.Set("b1", byte_value_1_->DeepCopy());
  to_set.Set("b2", byte_value_1_->DeepCopy());
  storage_->Set(DEFAULTS, to_set);
  settings.Set("b1", byte_value_1_->DeepCopy());
  settings.Set("b2", byte_value_1_->DeepCopy());
  storage_->Set(DEFAULTS, "b3", *byte_value_1_);
  settings.Set("b3", byte_value_1_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  // Remove some settings that don't exist.
  to_remove.clear();
  to_remove.push_back("a1");
  to_remove.push_back("a2");
  EXPECT_FALSE(storage_->Remove(to_remove)->HasError());
  EXPECT_FALSE(storage_->Remove("b")->HasError());
  EXPECT_TRUE(SettingsEqual(settings));

  // Still no quota.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, Clear) {
  DictionaryValue settings;
  CreateStorage(40, UINT_MAX, 5);

  // Test running out of byte quota.
  {
    DictionaryValue to_set;
    to_set.Set("a", byte_value_16_->DeepCopy());
    to_set.Set("b", byte_value_16_->DeepCopy());
    EXPECT_FALSE(storage_->Set(DEFAULTS, to_set)->HasError());
    EXPECT_TRUE(storage_->Set(DEFAULTS, "c", *byte_value_16_)->HasError());

    EXPECT_FALSE(storage_->Clear()->HasError());

    // (repeat)
    EXPECT_FALSE(storage_->Set(DEFAULTS, to_set)->HasError());
    EXPECT_TRUE(storage_->Set(DEFAULTS, "c", *byte_value_16_)->HasError());
  }

  // Test reaching max keys.
  storage_->Clear();
  {
    DictionaryValue to_set;
    to_set.Set("a", byte_value_1_->DeepCopy());
    to_set.Set("b", byte_value_1_->DeepCopy());
    to_set.Set("c", byte_value_1_->DeepCopy());
    to_set.Set("d", byte_value_1_->DeepCopy());
    to_set.Set("e", byte_value_1_->DeepCopy());
    EXPECT_FALSE(storage_->Set(DEFAULTS, to_set)->HasError());
    EXPECT_TRUE(storage_->Set(DEFAULTS, "f", *byte_value_1_)->HasError());

    storage_->Clear();

    // (repeat)
    EXPECT_FALSE(storage_->Set(DEFAULTS, to_set)->HasError());
    EXPECT_TRUE(storage_->Set(DEFAULTS, "f", *byte_value_1_)->HasError());
  }
}

TEST_F(ExtensionSettingsQuotaTest, ChangingUsedBytesWithSet) {
  DictionaryValue settings;
  CreateStorage(20, UINT_MAX, UINT_MAX);

  // Change a setting to make it go over quota.
  storage_->Set(DEFAULTS, "a", *byte_value_16_);
  settings.Set("a", byte_value_16_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_256_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));

  // Change a setting to reduce usage and room for another setting.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "foobar", *byte_value_1_)->HasError());
  storage_->Set(DEFAULTS, "a", *byte_value_1_);
  settings.Set("a", byte_value_1_->DeepCopy());

  EXPECT_FALSE(storage_->Set(DEFAULTS, "foobar", *byte_value_1_)->HasError());
  settings.Set("foobar", byte_value_1_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, SetsOnlyEntirelyCompletedWithByteQuota) {
  DictionaryValue settings;
  CreateStorage(40, UINT_MAX, UINT_MAX);

  storage_->Set(DEFAULTS, "a", *byte_value_16_);
  settings.Set("a", byte_value_16_->DeepCopy());

  // The entire change is over quota.
  DictionaryValue to_set;
  to_set.Set("b", byte_value_16_->DeepCopy());
  to_set.Set("c", byte_value_16_->DeepCopy());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));

  // The entire change is over quota, but quota reduced in existing key.
  to_set.Set("a", byte_value_1_->DeepCopy());
  EXPECT_FALSE(storage_->Set(DEFAULTS, to_set)->HasError());
  settings.Set("a", byte_value_1_->DeepCopy());
  settings.Set("b", byte_value_16_->DeepCopy());
  settings.Set("c", byte_value_16_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, SetsOnlyEntireCompletedWithMaxKeys) {
  DictionaryValue settings;
  CreateStorage(UINT_MAX, UINT_MAX, 2);

  storage_->Set(DEFAULTS, "a", *byte_value_1_);
  settings.Set("a", byte_value_1_->DeepCopy());

  DictionaryValue to_set;
  to_set.Set("b", byte_value_16_->DeepCopy());
  to_set.Set("c", byte_value_16_->DeepCopy());
  EXPECT_TRUE(storage_->Set(DEFAULTS, to_set)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, WithInitialDataAndByteQuota) {
  DictionaryValue settings;
  delegate_->Set(DEFAULTS, "a", *byte_value_256_);
  settings.Set("a", byte_value_256_->DeepCopy());

  CreateStorage(280, UINT_MAX, UINT_MAX);
  EXPECT_TRUE(SettingsEqual(settings));

  // Add some data.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", *byte_value_16_)->HasError());
  settings.Set("b", byte_value_16_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  // Not enough quota.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", *byte_value_16_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));

  // Reduce usage of original setting so that "c" can fit.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", *byte_value_16_)->HasError());
  settings.Set("a", byte_value_16_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", *byte_value_16_)->HasError());
  settings.Set("c", byte_value_16_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  // Remove to free up some more data.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "d", *byte_value_256_)->HasError());

  std::vector<std::string> to_remove;
  to_remove.push_back("a");
  to_remove.push_back("b");
  storage_->Remove(to_remove);
  settings.Remove("a", NULL);
  settings.Remove("b", NULL);
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "d", *byte_value_256_)->HasError());
  settings.Set("d", byte_value_256_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, WithInitialDataAndMaxKeys) {
  DictionaryValue settings;
  delegate_->Set(DEFAULTS, "a", *byte_value_1_);
  settings.Set("a", byte_value_1_->DeepCopy());
  CreateStorage(UINT_MAX, UINT_MAX, 2);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", *byte_value_1_)->HasError());
  settings.Set("b", byte_value_1_->DeepCopy());

  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", *byte_value_1_)->HasError());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, InitiallyOverByteQuota) {
  DictionaryValue settings;
  settings.Set("a", byte_value_16_->DeepCopy());
  settings.Set("b", byte_value_16_->DeepCopy());
  settings.Set("c", byte_value_16_->DeepCopy());
  delegate_->Set(DEFAULTS, settings);

  CreateStorage(40, UINT_MAX, UINT_MAX);
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_TRUE(storage_->Set(DEFAULTS, "d", *byte_value_16_)->HasError());

  // Take under quota by reducing size of an existing setting
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  settings.Set("a", byte_value_1_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able set another small setting.
  EXPECT_FALSE(storage_->Set(DEFAULTS, "d", *byte_value_1_)->HasError());
  settings.Set("d", byte_value_1_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, InitiallyOverMaxKeys) {
  DictionaryValue settings;
  settings.Set("a", byte_value_16_->DeepCopy());
  settings.Set("b", byte_value_16_->DeepCopy());
  settings.Set("c", byte_value_16_->DeepCopy());
  delegate_->Set(DEFAULTS, settings);

  CreateStorage(UINT_MAX, UINT_MAX, 2);
  EXPECT_TRUE(SettingsEqual(settings));

  // Can't set either an existing or new setting.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "d", *byte_value_16_)->HasError());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));

  // Should be able after removing 2.
  storage_->Remove("a");
  settings.Remove("a", NULL);
  storage_->Remove("b");
  settings.Remove("b", NULL);
  EXPECT_TRUE(SettingsEqual(settings));

  EXPECT_FALSE(storage_->Set(DEFAULTS, "e", *byte_value_1_)->HasError());
  settings.Set("e", byte_value_1_->DeepCopy());
  EXPECT_TRUE(SettingsEqual(settings));

  // Still can't set any.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "d", *byte_value_16_)->HasError());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, ZeroQuotaBytesPerSetting) {
  DictionaryValue empty;
  CreateStorage(UINT_MAX, 0, UINT_MAX);

  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Remove("a")->HasError());
  EXPECT_FALSE(storage_->Remove("b")->HasError());
  EXPECT_TRUE(SettingsEqual(empty));
}

TEST_F(ExtensionSettingsQuotaTest, QuotaBytesPerSetting) {
  DictionaryValue settings;

  CreateStorage(UINT_MAX, 20, UINT_MAX);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", *byte_value_16_)->HasError());
  settings.Set("a", byte_value_16_->DeepCopy());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_256_)->HasError());

  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", *byte_value_16_)->HasError());
  settings.Set("b", byte_value_16_->DeepCopy());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", *byte_value_256_)->HasError());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, QuotaBytesPerSettingWithInitialSettings) {
  DictionaryValue settings;

  delegate_->Set(DEFAULTS, "a", *byte_value_1_);
  delegate_->Set(DEFAULTS, "b", *byte_value_16_);
  delegate_->Set(DEFAULTS, "c", *byte_value_256_);
  CreateStorage(UINT_MAX, 20, UINT_MAX);

  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "a", *byte_value_16_)->HasError());
  settings.Set("a", byte_value_16_->DeepCopy());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "a", *byte_value_256_)->HasError());

  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "b", *byte_value_16_)->HasError());
  settings.Set("b", byte_value_16_->DeepCopy());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "b", *byte_value_256_)->HasError());

  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Set(DEFAULTS, "c", *byte_value_16_)->HasError());
  settings.Set("c", byte_value_16_->DeepCopy());
  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", *byte_value_256_)->HasError());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest,
    QuotaBytesPerSettingWithInitialSettingsForced) {
  // This is a lazy test to make sure IGNORE_QUOTA lets through changes: the
  // test above copied, but using IGNORE_QUOTA and asserting nothing is ever
  // rejected...
  DictionaryValue settings;

  delegate_->Set(DEFAULTS, "a", *byte_value_1_);
  delegate_->Set(DEFAULTS, "b", *byte_value_16_);
  delegate_->Set(DEFAULTS, "c", *byte_value_256_);
  CreateStorage(UINT_MAX, 20, UINT_MAX);

  EXPECT_FALSE(storage_->Set(IGNORE_QUOTA, "a", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Set(IGNORE_QUOTA, "a", *byte_value_16_)->HasError());
  EXPECT_FALSE(storage_->Set(IGNORE_QUOTA, "a", *byte_value_256_)->HasError());
  settings.Set("a", byte_value_256_->DeepCopy());

  EXPECT_FALSE(storage_->Set(IGNORE_QUOTA, "b", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Set(IGNORE_QUOTA, "b", *byte_value_16_)->HasError());
  EXPECT_FALSE(storage_->Set(IGNORE_QUOTA, "b", *byte_value_256_)->HasError());
  settings.Set("b", byte_value_256_->DeepCopy());

  EXPECT_FALSE(storage_->Set(IGNORE_QUOTA, "c", *byte_value_1_)->HasError());
  EXPECT_FALSE(storage_->Set(IGNORE_QUOTA, "c", *byte_value_16_)->HasError());
  settings.Set("c", byte_value_16_->DeepCopy());

  // ... except the last.  Make sure it can still fail.
  EXPECT_TRUE(storage_->Set(DEFAULTS, "c", *byte_value_256_)->HasError());

  EXPECT_TRUE(SettingsEqual(settings));
}

TEST_F(ExtensionSettingsQuotaTest, GetBytesInUse) {
  // Just testing GetBytesInUse, no need for a quota.
  CreateStorage(UINT_MAX, UINT_MAX, UINT_MAX);

  std::vector<std::string> ab;
  ab.push_back("a");
  ab.push_back("b");

  EXPECT_EQ(0u, storage_->GetBytesInUse());
  EXPECT_EQ(0u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(0u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(0u, storage_->GetBytesInUse(ab));

  storage_->Set(DEFAULTS, "a", *byte_value_1_);

  EXPECT_EQ(2u, storage_->GetBytesInUse());
  EXPECT_EQ(2u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(0u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(2u, storage_->GetBytesInUse(ab));

  storage_->Set(DEFAULTS, "b", *byte_value_1_);

  EXPECT_EQ(4u, storage_->GetBytesInUse());
  EXPECT_EQ(2u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(2u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(4u, storage_->GetBytesInUse(ab));

  storage_->Set(DEFAULTS, "c", *byte_value_1_);

  EXPECT_EQ(6u, storage_->GetBytesInUse());
  EXPECT_EQ(2u, storage_->GetBytesInUse("a"));
  EXPECT_EQ(2u, storage_->GetBytesInUse("b"));
  EXPECT_EQ(4u, storage_->GetBytesInUse(ab));
}

}  // namespace extensions
