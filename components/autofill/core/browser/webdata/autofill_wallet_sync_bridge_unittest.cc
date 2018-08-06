// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/webdata/common/web_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using base::ScopedTempDir;
using sync_pb::AutofillWalletSpecifics;
using syncer::DataBatch;
using syncer::EntityData;
using syncer::EntityDataPtr;
using syncer::KeyAndData;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelType;
using testing::UnorderedElementsAre;

// Non-UTF8 server IDs.
const char kAddr1ServerId[] = "addr1\xEF\xBF\xBE";
const char kAddr2ServerId[] = "addr2\xEF\xBF\xBE";
const char kCard1ServerId[] = "card1\xEF\xBF\xBE";
const char kCard2ServerId[] = "card2\xEF\xBF\xBE";

// Base64 encodings of the server IDs, used as ids in WalletMetadataSpecifics
// (these are suitable for syncing, because they are valid UTF-8).
const char kAddr1SpecificsId[] = "YWRkcjHvv74=";
//const char kAddr2SpecificsId[] = "YWRkcjLvv74=";
const char kCard1SpecificsId[] = "Y2FyZDHvv74=";
//const char kCard2SpecificsId[] = "Y2FyZDLvv74=";

// Unique sync tags for the server IDs.
const char kAddr1SyncTag[] = "address-YWRkcjHvv74=";
const char kCard1SyncTag[] = "card-Y2FyZDHvv74=";

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

AutofillWalletSpecifics CreateAutofillWalletSpecificsForCard(
    const std::string& specifics_id) {
  sync_pb::AutofillWalletSpecifics wallet_specifics;
  wallet_specifics.set_type(
      sync_pb::AutofillWalletSpecifics_WalletInfoType::
          AutofillWalletSpecifics_WalletInfoType_MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* card_specifics =
      wallet_specifics.mutable_masked_card();
  card_specifics->set_id(specifics_id);
  return wallet_specifics;
}

AutofillWalletSpecifics CreateAutofillWalletSpecificsForAddress(
    const std::string& specifics_id) {
  sync_pb::AutofillWalletSpecifics wallet_specifics;
  wallet_specifics.set_type(
      sync_pb::AutofillWalletSpecifics_WalletInfoType::
          AutofillWalletSpecifics_WalletInfoType_POSTAL_ADDRESS);

  sync_pb::WalletPostalAddress* address_specifics =
      wallet_specifics.mutable_address();
  address_specifics->set_id(specifics_id);
  return wallet_specifics;
}

void ExtractAutofillWalletSpecificsFromDataBatch(
    std::unique_ptr<DataBatch> batch,
    std::vector<AutofillWalletSpecifics>* output) {
  while (batch->HasNext()) {
    const KeyAndData& data_pair = batch->Next();
    output->push_back(data_pair.second->specifics.autofill_wallet());
  }
}

std::string WalletMaskedCreditCardSpecificsAsDebugString(
    const AutofillWalletSpecifics& specifics) {
  std::ostringstream output;
  output << "[id: " << specifics.masked_card().id()
         << ", type: " << static_cast<int>(specifics.type())
         << ", name_on_card: " << specifics.masked_card().name_on_card()
         << ", type: " << specifics.masked_card().type()
         << ", last_four: " << specifics.masked_card().last_four()
         << ", exp_month: " << specifics.masked_card().exp_month()
         << ", exp_year: " << specifics.masked_card().exp_year()
         << ", billing_address_id: "
         << specifics.masked_card().billing_address_id()
         << ", card_class: " << specifics.masked_card().card_class()
         << ", bank_name: " << specifics.masked_card().bank_name() << "]";
  return output.str();
}

std::string WalletPostalAddressSpecificsAsDebugString(
    const AutofillWalletSpecifics& specifics) {
  std::ostringstream output;
  output << "[id: " << specifics.address().id()
         << ", type: " << static_cast<int>(specifics.type())
         << ", recipient_name: " << specifics.address().recipient_name()
         << ", company_name: " << specifics.address().company_name()
         << ", street_address: "
         << (specifics.address().street_address_size()
                 ? specifics.address().street_address(0)
                 : "")
         << ", address_1: " << specifics.address().address_1()
         << ", address_2: " << specifics.address().address_2()
         << ", address_3: " << specifics.address().address_3()
         << ", postal_code: " << specifics.address().postal_code()
         << ", country_code: " << specifics.address().country_code()
         << ", phone_number: " << specifics.address().phone_number()
         << ", sorting_code: " << specifics.address().sorting_code() << "]";
  return output.str();
}

std::string AutofillWalletSpecificsAsDebugString(
    const AutofillWalletSpecifics& specifics) {
  switch (specifics.type()) {
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_MASKED_CREDIT_CARD:
      return WalletMaskedCreditCardSpecificsAsDebugString(specifics);
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_POSTAL_ADDRESS:
      return WalletPostalAddressSpecificsAsDebugString(specifics);
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_CUSTOMER_DATA:
      return "CustomerData";
    case sync_pb::AutofillWalletSpecifics_WalletInfoType::
        AutofillWalletSpecifics_WalletInfoType_UNKNOWN:
      return "Unknown";
  }
}

MATCHER_P(EqualsSpecifics, expected, "") {
  if (arg.SerializeAsString() != expected.SerializeAsString()) {
    *result_listener << "entry\n"
                     << AutofillWalletSpecificsAsDebugString(arg) << "\n"
                     << "did not match expected\n"
                     << AutofillWalletSpecificsAsDebugString(expected);
    return false;
  }
  return true;
}

}  // namespace

class AutofillWalletSyncBridgeTest : public testing::Test {
 public:
  AutofillWalletSyncBridgeTest() {}
  ~AutofillWalletSyncBridgeTest() override {}

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
            syncer::AUTOFILL_WALLET_DATA, /*dump_stack=*/base::DoNothing(),
            /*commit_only=*/false);
    mock_processor_.DelegateCallsByDefaultTo(real_processor_.get());
  }

  void ResetBridge() {
    bridge_.reset(new AutofillWalletSyncBridge(
        mock_processor_.CreateForwardingProcessor(), &backend_));
  }

  EntityData SpecificsToEntity(const AutofillWalletSpecifics& specifics) {
    EntityData data;
    *data.specifics.mutable_autofill_wallet() = specifics;
    data.client_tag_hash = syncer::GenerateSyncableHash(
        syncer::AUTOFILL_WALLET_DATA, bridge()->GetClientTag(data));
    return data;
  }

  std::vector<AutofillWalletSpecifics> GetAllLocalData() {
    std::vector<AutofillWalletSpecifics> data;
    // Perform an async call synchronously for testing.
    base::RunLoop loop;
    bridge()->GetAllDataForDebugging(base::BindLambdaForTesting(
        [&loop, &data](std::unique_ptr<DataBatch> batch) {
          ExtractAutofillWalletSpecificsFromDataBatch(std::move(batch), &data);
          loop.Quit();
        }));
    loop.Run();
    return data;
  }

  AutofillWalletSyncBridge* bridge() { return bridge_.get(); }

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
  std::unique_ptr<AutofillWalletSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletSyncBridgeTest);
};

// The following 2 tests make sure client tags stay stable.
TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForAddress) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForAddress(kAddr1SyncTag);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            "address-" + std::string(kAddr1SyncTag));
}

TEST_F(AutofillWalletSyncBridgeTest, GetClientTagForCard) {
  AutofillWalletSpecifics specifics =
      CreateAutofillWalletSpecificsForCard(kCard1SyncTag);
  EXPECT_EQ(bridge()->GetClientTag(SpecificsToEntity(specifics)),
            "card-" + std::string(kCard1SyncTag));
}

// The following 2 tests make sure storage keys stay stable.
TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForAddress) {
  AutofillWalletSpecifics specifics1 =
      CreateAutofillWalletSpecificsForAddress(kAddr1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics1)),
            kAddr1SpecificsId);
}

TEST_F(AutofillWalletSyncBridgeTest, GetStorageKeyForCard) {
  AutofillWalletSpecifics specifics2 =
      CreateAutofillWalletSpecificsForCard(kCard1SpecificsId);
  EXPECT_EQ(bridge()->GetStorageKey(SpecificsToEntity(specifics2)),
            kCard1SpecificsId);
}

TEST_F(AutofillWalletSyncBridgeTest,
       GetAllDataForDebugging_ShouldReturnAllData) {
  AutofillProfile address1 = CreateServerProfile(kAddr1ServerId);
  AutofillProfile address2 = CreateServerProfile(kAddr2ServerId);
  table()->SetServerProfiles({address1, address2});
  CreditCard card1 = CreateServerCreditCard(kCard1ServerId);
  CreditCard card2 = CreateServerCreditCard(kCard2ServerId);
  table()->SetServerCreditCards({card1, card2});

  AutofillWalletSpecifics address_specifics1;
  SetAutofillWalletSpecificsFromServerProfile(address1, &address_specifics1);
  AutofillWalletSpecifics address_specifics2;
  SetAutofillWalletSpecificsFromServerProfile(address2, &address_specifics2);
  AutofillWalletSpecifics card_specifics1;
  SetAutofillWalletSpecificsFromServerCard(card1, &card_specifics1);
  AutofillWalletSpecifics card_specifics2;
  SetAutofillWalletSpecificsFromServerCard(card2, &card_specifics2);

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(address_specifics1),
                                   EqualsSpecifics(address_specifics2),
                                   EqualsSpecifics(card_specifics1),
                                   EqualsSpecifics(card_specifics2)));
}

}  // namespace autofill