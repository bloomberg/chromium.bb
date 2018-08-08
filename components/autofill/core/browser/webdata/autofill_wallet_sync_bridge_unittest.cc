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
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/credit_card.h"
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
using syncer::EntityChange;
using syncer::EntityData;
using syncer::EntityDataPtr;
using syncer::KeyAndData;
using syncer::MockModelTypeChangeProcessor;
using syncer::ModelType;
using testing::UnorderedElementsAre;

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

  void StartSyncing(
      const std::vector<AutofillWalletSpecifics>& remote_data = {}) {
    base::RunLoop loop;
    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    real_processor_->OnSyncStarting(
        request,
        base::BindLambdaForTesting(
            [&loop](std::unique_ptr<syncer::DataTypeActivationResponse>) {
              loop.Quit();
            }));
    loop.Run();

    // Initialize the processor with initial_sync_done.
    sync_pb::ModelTypeState state;
    state.set_initial_sync_done(true);
    syncer::UpdateResponseDataList initial_updates;
    for (const AutofillWalletSpecifics& specifics : remote_data) {
      initial_updates.push_back(SpecificsToUpdateResponse(specifics));
    }
    real_processor_->OnUpdateReceived(state, initial_updates);
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

  syncer::UpdateResponseData SpecificsToUpdateResponse(
      const AutofillWalletSpecifics& specifics) {
    syncer::UpdateResponseData data;
    data.entity = SpecificsToEntity(specifics).PassToPtr();
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
  AutofillProfile address1 = test::GetServerProfile();
  AutofillProfile address2 = test::GetServerProfile2();
  table()->SetServerProfiles({address1, address2});
  CreditCard card1 = test::GetMaskedServerCard();
  CreditCard card2 = test::GetMaskedServerCardAmex();
  table()->SetServerCreditCards({card1, card2});

  AutofillWalletSpecifics profile_specifics1;
  SetAutofillWalletSpecificsFromServerProfile(address1, &profile_specifics1);
  AutofillWalletSpecifics profile_specifics2;
  SetAutofillWalletSpecificsFromServerProfile(address2, &profile_specifics2);
  AutofillWalletSpecifics card_specifics1;
  SetAutofillWalletSpecificsFromServerCard(card1, &card_specifics1);
  AutofillWalletSpecifics card_specifics2;
  SetAutofillWalletSpecificsFromServerCard(card2, &card_specifics2);

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics1),
                                   EqualsSpecifics(profile_specifics2),
                                   EqualsSpecifics(card_specifics1),
                                   EqualsSpecifics(card_specifics2)));
}

// Tests that when a new wallet card and new wallet address are sent by the
// server, the client only keeps the new data.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_NewWalletAddressAndCard) {
  // Create one profile and one card on the client.
  AutofillProfile address1 = test::GetServerProfile();
  table()->SetServerProfiles({address1});
  CreditCard card1 = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card1});

  // Create a different profile and a different card on the server.
  AutofillProfile address2 = test::GetServerProfile2();
  AutofillWalletSpecifics profile_specifics2;
  SetAutofillWalletSpecificsFromServerProfile(address2, &profile_specifics2);
  CreditCard card2 = test::GetMaskedServerCardAmex();
  AutofillWalletSpecifics card_specifics2;
  SetAutofillWalletSpecificsFromServerCard(card2, &card_specifics2);

  StartSyncing({profile_specifics2, card_specifics2});

  // Only the server card should be present on the client.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics2),
                                   EqualsSpecifics(card_specifics2)));
}

// Tests that when the server sends no cards or address, the client should
// delete all it's existing data.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_NoWalletAddressOrCard) {
  // Create one profile and one card on the client.
  AutofillProfile local_profile = test::GetServerProfile();
  table()->SetServerProfiles({local_profile});
  CreditCard local_card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({local_card});

  StartSyncing({});

  EXPECT_TRUE(GetAllLocalData().empty());
}

// Test that when the server sends the same address and card as the client has,
// nothing changes on the client.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_SameWalletAddressAndCard) {
  // Create one profile and one card on the client.
  AutofillProfile profile = test::GetServerProfile();
  table()->SetServerProfiles({profile});
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});

  // Create the same profile and card on the server.
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(profile, &profile_specifics);
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);

  StartSyncing({profile_specifics, card_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics)));
}

// Tests that when there are multiple changes happening at the same time, the
// data from the server is what the client ends up with,
TEST_F(AutofillWalletSyncBridgeTest,
       MergeSyncData_AddRemoveAndPreserveWalletAddressAndCard) {
  // Create two profile and one card on the client.
  AutofillProfile profile = test::GetServerProfile();
  AutofillProfile profile2 = test::GetServerProfile2();
  table()->SetServerProfiles({profile, profile2});
  CreditCard card = test::GetMaskedServerCard();
  table()->SetServerCreditCards({card});

  // Create one of the same profiles and a different card on the server.
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(profile, &profile_specifics);
  // The Amex card has different values for the relevant fields.
  CreditCard card2 = test::GetMaskedServerCardAmex();
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);

  StartSyncing({profile_specifics, card_specifics});

  // Make sure that the client only has the data from the server.
  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics),
                                   EqualsSpecifics(card_specifics)));
}

// Test that all field values for a address sent form the server are copied on
// the address on the client.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_SetsAllWalletAddressData) {
  // Create a profile to be synced from the server.
  AutofillProfile profile = test::GetServerProfile();
  AutofillWalletSpecifics profile_specifics;
  SetAutofillWalletSpecificsFromServerProfile(profile, &profile_specifics);

  StartSyncing({profile_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(profile_specifics)));

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  table()->GetServerProfiles(&profiles);
  ASSERT_EQ(1U, profiles.size());

  // Make sure that all the data was set properly.
  EXPECT_EQ(profile.GetRawInfo(NAME_FULL), profiles[0]->GetRawInfo(NAME_FULL));
  EXPECT_EQ(profile.GetRawInfo(COMPANY_NAME),
            profiles[0]->GetRawInfo(COMPANY_NAME));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            profiles[0]->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_STATE),
            profiles[0]->GetRawInfo(ADDRESS_HOME_STATE));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_CITY),
            profiles[0]->GetRawInfo(ADDRESS_HOME_CITY));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY),
            profiles[0]->GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_ZIP),
            profiles[0]->GetRawInfo(ADDRESS_HOME_ZIP));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_COUNTRY),
            profiles[0]->GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
            profiles[0]->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_EQ(profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE),
            profiles[0]->GetRawInfo(ADDRESS_HOME_SORTING_CODE));
  EXPECT_EQ(profile.language_code(), profiles[0]->language_code());

  // Also make sure that those types are not empty, to exercice all the code
  // paths.
  EXPECT_FALSE(profile.GetRawInfo(NAME_FULL).empty());
  EXPECT_FALSE(profile.GetRawInfo(COMPANY_NAME).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_STATE).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_CITY).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_ZIP).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_COUNTRY).empty());
  EXPECT_FALSE(profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER).empty());
  EXPECT_FALSE(profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE).empty());
  EXPECT_FALSE(profile.language_code().empty());
}

// Test that all field values for a card sent form the server are copied on the
// card on the client.
TEST_F(AutofillWalletSyncBridgeTest, MergeSyncData_SetsAllWalletCardData) {
  // Create a card to be synced from the server.
  CreditCard card = test::GetMaskedServerCard();
  // Add this value type as it is not added by default but should be synced.
  card.set_bank_name("The Bank");
  AutofillWalletSpecifics card_specifics;
  SetAutofillWalletSpecificsFromServerCard(card, &card_specifics);

  StartSyncing({card_specifics});

  EXPECT_THAT(GetAllLocalData(),
              UnorderedElementsAre(EqualsSpecifics(card_specifics)));

  std::vector<std::unique_ptr<CreditCard>> cards;
  table()->GetServerCreditCards(&cards);
  ASSERT_EQ(1U, cards.size());

  // Make sure that all the data was set properly.
  EXPECT_EQ(card.network(), cards[0]->network());
  EXPECT_EQ(card.LastFourDigits(), cards[0]->LastFourDigits());
  EXPECT_EQ(card.expiration_month(), cards[0]->expiration_month());
  EXPECT_EQ(card.expiration_year(), cards[0]->expiration_year());
  EXPECT_EQ(card.billing_address_id(), cards[0]->billing_address_id());
  EXPECT_EQ(card.card_type(), cards[0]->card_type());
  EXPECT_EQ(card.bank_name(), cards[0]->bank_name());

  // Also make sure that those types are not empty, to exercice all the code
  // paths.
  EXPECT_FALSE(card.network().empty());
  EXPECT_FALSE(card.LastFourDigits().empty());
  EXPECT_NE(0, card.expiration_month());
  EXPECT_NE(0, card.expiration_year());
  EXPECT_FALSE(card.billing_address_id().empty());
  EXPECT_NE(CreditCard::CARD_TYPE_UNKNOWN, card.card_type());
  EXPECT_FALSE(card.bank_name().empty());
}

}  // namespace autofill