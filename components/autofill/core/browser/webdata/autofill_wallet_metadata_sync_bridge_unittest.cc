// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::ScopedTempDir;
using sync_pb::WalletMetadataSpecifics;
using syncer::EntityData;
using syncer::EntityDataPtr;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelType;

// Base64 encoded SHA1 hash of all address fields.
const char kAddressHashA[] = "MgM+9iWXwMGwEpFfDDp06K3jizU=";

// Base64 encoded string (opaque to Chrome; any such string works here).
const char kCardIdA[] = "AQIDBAECAwQBAgMEAQIDBAECAwQBAgMEAQIDBAECAwQBAgME";

const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

class FakeAutofillBackend : public AutofillWebDataBackend {
 public:
  FakeAutofillBackend() {}
  ~FakeAutofillBackend() override {}
  WebDatabase* GetDatabase() override { return db_; }
  void AddObserver(
      autofill::AutofillWebDataServiceObserverOnDBSequence* observer) override {
  }
  void RemoveObserver(
      autofill::AutofillWebDataServiceObserverOnDBSequence* observer) override {
  }
  void RemoveExpiredFormElements() override {}
  void NotifyOfMultipleAutofillChanges() override {}
  void NotifyThatSyncHasStarted(ModelType model_type) override {}
  void SetWebDatabase(WebDatabase* db) { db_ = db; }

 private:
  WebDatabase* db_;
};

WalletMetadataSpecifics CreateWalletMetadataSpecificsForAddress(
    const std::string& id) {
  WalletMetadataSpecifics specifics;
  specifics.set_id(id);
  specifics.set_type(WalletMetadataSpecifics::ADDRESS);
  return specifics;
}

WalletMetadataSpecifics CreateWalletMetadataSpecificsForCard(
    const std::string& id) {
  WalletMetadataSpecifics specifics;
  specifics.set_id(id);
  specifics.set_type(WalletMetadataSpecifics::CARD);
  return specifics;
}

}  // namespace

class AutofillWalletMetadataSyncBridgeTest : public testing::Test {
 public:
  AutofillWalletMetadataSyncBridgeTest() {}
  ~AutofillWalletMetadataSyncBridgeTest() override {}

  void SetUp() override {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
    CountryNames::SetLocaleString(kLocaleString);
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    db_.Init(temp_dir_.GetPath().AppendASCII("SyncTestWebDatabase"));
    backend_.SetWebDatabase(&db_);
    ResetProcessor();
    ResetBridge();
  }

  void ResetProcessor() {
    real_processor_ =
        std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
            syncer::AUTOFILL_WALLET_METADATA, /*dump_stack=*/base::DoNothing(),
            /*commit_only=*/false);
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge() {
    bridge_.reset(new AutofillWalletMetadataSyncBridge(
        mock_processor_.CreateForwardingProcessor()));
  }

  EntityData SpecificsToEntity(const WalletMetadataSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_wallet_metadata() = specifics;
    data.client_tag_hash = syncer::GenerateSyncableHash(
        syncer::AUTOFILL_WALLET_METADATA, bridge()->GetClientTag(data));
    return data;
  }

  AutofillWalletMetadataSyncBridge* bridge() { return bridge_.get(); }

  syncer::MockModelTypeChangeProcessor& mock_processor() {
    return mock_processor_;
  }

  AutofillTable* table() { return &table_; }

  FakeAutofillBackend* backend() { return &backend_; }

 private:
  autofill::TestAutofillClock test_clock_;
  ScopedTempDir temp_dir_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  FakeAutofillBackend backend_;
  AutofillTable table_;
  WebDatabase db_;
  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;
  std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor> real_processor_;
  std::unique_ptr<AutofillWalletMetadataSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletMetadataSyncBridgeTest);
};

// The following 2 tests make sure client tags stay stable.
TEST_F(AutofillWalletMetadataSyncBridgeTest, GetClientTagForAddress) {
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForAddress(kAddressHashA);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            "address-" + std::string(kAddressHashA));
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetClientTagForCard) {
  WalletMetadataSpecifics specifics =
      CreateWalletMetadataSpecificsForCard(kCardIdA);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            "card-" + std::string(kCardIdA));
}

// The following 2 tests make sure storage keys stay stable.
TEST_F(AutofillWalletMetadataSyncBridgeTest, GetStorageKeyForAddress) {
  WalletMetadataSpecifics specifics1 =
      CreateWalletMetadataSpecificsForAddress(kAddressHashA);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics1)),
            kAddressHashA);
}

TEST_F(AutofillWalletMetadataSyncBridgeTest, GetStorageKeyForCard) {
  WalletMetadataSpecifics specifics2 =
      CreateWalletMetadataSpecificsForCard(kCardIdA);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics2)), kCardIdA);
}

}  // namespace autofill
