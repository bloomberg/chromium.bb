// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_syncable_service.h"

#include <utility>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor_wrapper_for_test.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/autofill_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using testing::DoAll;
using testing::ElementsAre;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::UnorderedElementsAre;
using testing::Value;
using testing::_;

ACTION_P2(GetPointersTo, profiles, cards) {
  for (auto& profile : *profiles)
    arg0->insert(std::make_pair(profile.server_id(), &profile));

  for (auto& card : *cards)
    arg1->insert(std::make_pair(card.server_id(), &card));
}

ACTION_P(SaveDataIn, list) {
  for (auto it = list->begin(); it != list->end(); ++it) {
    if (it->server_id() == arg0.server_id()) {
      *it = arg0;
      return;
    }
  }

  list->push_back(arg0);
}

// A syncable service for Wallet metadata that mocks out disk IO.
class MockService : public AutofillWalletMetadataSyncableService {
 public:
  MockService()
      : AutofillWalletMetadataSyncableService(nullptr, std::string()) {
    ON_CALL(*this, GetLocalData(_, _))
        .WillByDefault(DoAll(GetPointersTo(&server_profiles_, &server_cards_),
                             Return(true)));

    ON_CALL(*this, UpdateAddressStats(_))
        .WillByDefault(DoAll(SaveDataIn(&server_profiles_), Return(true)));

    ON_CALL(*this, UpdateCardStats(_))
        .WillByDefault(DoAll(SaveDataIn(&server_cards_), Return(true)));

    ON_CALL(*this, SendChangesToSyncServer(_))
        .WillByDefault(
            Invoke(this, &MockService::SendChangesToSyncServerConcrete));
  }

  ~MockService() override {}

  MOCK_METHOD1(UpdateAddressStats, bool(const AutofillProfile&));
  MOCK_METHOD1(UpdateCardStats, bool(const CreditCard&));
  MOCK_METHOD1(SendChangesToSyncServer,
               syncer::SyncError(const syncer::SyncChangeList&));

  void ClearServerData() {
    server_profiles_.clear();
    server_cards_.clear();
  }

 private:
  MOCK_CONST_METHOD2(GetLocalData,
                     bool(std::map<std::string, AutofillProfile*>*,
                          std::map<std::string, CreditCard*>*));

  syncer::SyncError SendChangesToSyncServerConcrete(
      const syncer::SyncChangeList& changes) {
    return AutofillWalletMetadataSyncableService::SendChangesToSyncServer(
        changes);
  }

  syncer::SyncDataList GetAllSyncDataConcrete(syncer::ModelType type) const {
    return AutofillWalletMetadataSyncableService::GetAllSyncData(type);
  }

  std::vector<AutofillProfile> server_profiles_;
  std::vector<CreditCard> server_cards_;

  DISALLOW_COPY_AND_ASSIGN(MockService);
};

// Verify that nothing is sent to the sync server when there's no metadata on
// disk.
TEST(AutofillWalletMetadataSyncableServiceTest, NoMetadataToReturn) {
  EXPECT_TRUE(NiceMock<MockService>()
                  .GetAllSyncData(syncer::AUTOFILL_WALLET_METADATA)
                  .empty());
}

AutofillProfile BuildAddress(const std::string& server_id,
                             int64 use_count,
                             int64 use_date) {
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, server_id);
  profile.set_use_count(use_count);
  profile.set_use_date(base::Time::FromInternalValue(use_date));
  return profile;
}

CreditCard BuildCard(const std::string& server_id,
                     int64 use_count,
                     int64 use_date) {
  CreditCard card(CreditCard::MASKED_SERVER_CARD, server_id);
  card.set_use_count(use_count);
  card.set_use_date(base::Time::FromInternalValue(use_date));
  return card;
}

MATCHER_P5(SyncDataMatches,
           sync_tag,
           metadata_type,
           server_id,
           use_count,
           use_date,
           "") {
  return arg.IsValid() &&
         syncer::AUTOFILL_WALLET_METADATA == arg.GetDataType() &&
         sync_tag == syncer::SyncDataLocal(arg).GetTag() &&
         metadata_type == arg.GetSpecifics().wallet_metadata().type() &&
         server_id == arg.GetSpecifics().wallet_metadata().id() &&
         use_count == arg.GetSpecifics().wallet_metadata().use_count() &&
         use_date == arg.GetSpecifics().wallet_metadata().use_date();
}

// Verify that all metadata from disk is sent to the sync server.
TEST(AutofillWalletMetadataSyncableServiceTest, ReturnAllMetadata) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));

  EXPECT_THAT(
      local.GetAllSyncData(syncer::AUTOFILL_WALLET_METADATA),
      UnorderedElementsAre(
          SyncDataMatches("address-addr",
                          sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 1,
                          2),
          SyncDataMatches("card-card", sync_pb::WalletMetadataSpecifics::CARD,
                          "card", 3, 4)));
}

void MergeMetadata(MockService* local, MockService* remote) {
  // The wrapper for |remote| gives it a null change processor, so sending
  // changes is not possible.
  ON_CALL(*remote, SendChangesToSyncServer(_))
      .WillByDefault(Return(syncer::SyncError()));

  scoped_ptr<syncer::SyncErrorFactoryMock> errors(
      new syncer::SyncErrorFactoryMock);
  EXPECT_CALL(*errors, CreateAndUploadError(_, _)).Times(0);
  EXPECT_FALSE(
      local->MergeDataAndStartSyncing(
               syncer::AUTOFILL_WALLET_METADATA,
               remote->GetAllSyncData(syncer::AUTOFILL_WALLET_METADATA),
               scoped_ptr<syncer::SyncChangeProcessor>(
                   new syncer::SyncChangeProcessorWrapperForTest(remote)),
               errors.Pass())
          .error()
          .IsSet());
}

// Verify that nothing is written to disk or sent to the sync server when two
// empty clients are syncing.
TEST(AutofillWalletMetadataSyncableServiceTest, TwoEmptyClients) {
  NiceMock<MockService> local;
  NiceMock<MockService> remote;

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  MergeMetadata(&local, &remote);
}

MATCHER_P2(SyncChangeMatches, change_type, sync_tag, "") {
  return arg.IsValid() && change_type == arg.change_type() &&
         sync_tag == syncer::SyncDataLocal(arg.sync_data()).GetTag() &&
         syncer::AUTOFILL_WALLET_METADATA == arg.sync_data().GetDataType();
}

// Verify that remote data without local counterpart is deleted during the
// initial merge.
TEST(AutofillWalletMetadataSyncableServiceTest, DeleteFromServerOnMerge) {
  NiceMock<MockService> local;
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(
      local,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, "address-addr"),
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, "card-card"))));

  MergeMetadata(&local, &remote);
}

MATCHER_P6(SyncChangeAndDataMatch,
           change_type,
           sync_tag,
           metadata_type,
           server_id,
           use_count,
           use_date,
           "") {
  return Value(arg, SyncChangeMatches(change_type, sync_tag)) &&
         Value(arg.sync_data(),
               SyncDataMatches(sync_tag, metadata_type, server_id, use_count,
                               use_date));
}

// Verify that local data is sent to the sync server during the initial merge,
// if the server does not have the data already.
TEST(AutofillWalletMetadataSyncableServiceTest, AddToServerOnMerge) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local,
              SendChangesToSyncServer(UnorderedElementsAre(
                  SyncChangeAndDataMatch(
                      syncer::SyncChange::ACTION_ADD, "address-addr",
                      sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 1, 2),
                  SyncChangeAndDataMatch(
                      syncer::SyncChange::ACTION_ADD, "card-card",
                      sync_pb::WalletMetadataSpecifics::CARD, "card", 3, 4))));

  MergeMetadata(&local, &remote);
}

// Verify that no data is written to disk or sent to the sync server if the
// local and remote data are identical during the initial merge.
TEST(AutofillWalletMetadataSyncableServiceTest, IgnoreIdenticalValuesOnMerge) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  MergeMetadata(&local, &remote);
}

MATCHER_P3(AutofillMetadataMatches, server_id, use_count, use_date, "") {
  return arg.server_id() == server_id &&
         arg.use_count() == static_cast<size_t>(use_count) &&
         arg.use_date() == base::Time::FromInternalValue(use_date);
}

// Verify that remote data with higher values of use count and last use date is
// saved to disk during the initial merge.
TEST(AutofillWalletMetadataSyncableServiceTest,
     SaveHigherValuesLocallyOnMerge) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 10, 20));
  remote.UpdateCardStats(BuildCard("card", 30, 40));

  EXPECT_CALL(local,
              UpdateAddressStats(AutofillMetadataMatches("addr", 10, 20)));
  EXPECT_CALL(local, UpdateCardStats(AutofillMetadataMatches("card", 30, 40)));
  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  MergeMetadata(&local, &remote);
}

// Verify that local data with higher values of use count and last use date is
// sent to the sync server during the initial merge.
TEST(AutofillWalletMetadataSyncableServiceTest,
     SendHigherValuesToServerOnMerge) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 10, 20));
  local.UpdateCardStats(BuildCard("card", 30, 40));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(
      local, SendChangesToSyncServer(UnorderedElementsAre(
                 SyncChangeAndDataMatch(
                     syncer::SyncChange::ACTION_UPDATE, "address-addr",
                     sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 10, 20),
                 SyncChangeAndDataMatch(
                     syncer::SyncChange::ACTION_UPDATE, "card-card",
                     sync_pb::WalletMetadataSpecifics::CARD, "card", 30, 40))));

  MergeMetadata(&local, &remote);
}

// Verify that lower values of metadata are not sent to the sync server when
// local metadata is updated.
TEST(AutofillWalletMetadataSyncableServiceTest,
     DontSendLowerValueToServerOnSingleChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  AutofillProfile address = BuildAddress("addr", 0, 0);
  CreditCard card = BuildCard("card", 0, 0);

  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  local.AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, address.guid(), &address));
  local.CreditCardChanged(
      CreditCardChange(CreditCardChange::UPDATE, card.guid(), &card));
}

// Verify that higher values of metadata are sent to the sync server when local
// metadata is updated.
TEST(AutofillWalletMetadataSyncableServiceTest,
     SendHigherValuesToServerOnLocalSingleChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  AutofillProfile address = BuildAddress("addr", 10, 20);
  CreditCard card = BuildCard("card", 30, 40);

  EXPECT_CALL(local,
              SendChangesToSyncServer(ElementsAre(SyncChangeAndDataMatch(
                  syncer::SyncChange::ACTION_UPDATE, "address-addr",
                  sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 10, 20))));
  EXPECT_CALL(local,
              SendChangesToSyncServer(ElementsAre(SyncChangeAndDataMatch(
                  syncer::SyncChange::ACTION_UPDATE, "card-card",
                  sync_pb::WalletMetadataSpecifics::CARD, "card", 30, 40))));

  local.AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, address.guid(), &address));
  local.CreditCardChanged(
      CreditCardChange(CreditCardChange::UPDATE, card.guid(), &card));
}

// Verify that one-off addition of metadata is not sent to the sync
// server. Metadata add and delete trigger multiple changes notification
// instead.
TEST(AutofillWalletMetadataSyncableServiceTest, DontAddToServerOnSingleChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  AutofillProfile address = BuildAddress("new-addr", 5, 6);
  CreditCard card = BuildCard("new-card", 7, 8);

  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  local.AutofillProfileChanged(AutofillProfileChange(
      AutofillProfileChange::UPDATE, address.guid(), &address));
  local.CreditCardChanged(
      CreditCardChange(CreditCardChange::UPDATE, card.guid(), &card));
}

// Verify that new metadata is sent to the sync server when multiple metadata
// values change at once.
TEST(AutofillWalletMetadataSyncableServiceTest, AddToServerOnMultiChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  // These methods do not trigger notifications or sync:
  local.UpdateAddressStats(BuildAddress("new-addr", 5, 6));
  local.UpdateCardStats(BuildCard("new-card", 7, 8));

  EXPECT_CALL(
      local,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeAndDataMatch(
              syncer::SyncChange::ACTION_ADD, "address-new-addr",
              sync_pb::WalletMetadataSpecifics::ADDRESS, "new-addr", 5, 6),
          SyncChangeAndDataMatch(
              syncer::SyncChange::ACTION_ADD, "card-new-card",
              sync_pb::WalletMetadataSpecifics::CARD, "new-card", 7, 8))));

  local.AutofillMultipleChanged();
}

// Verify that higher values of existing metadata are sent to the sync server
// when multiple metadata values change at once.
TEST(AutofillWalletMetadataSyncableServiceTest,
     UpdateToHigherValueOnServerOnMultiChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  // These methods do not trigger notifications or sync:
  local.UpdateAddressStats(BuildAddress("addr", 5, 6));
  local.UpdateCardStats(BuildCard("card", 7, 8));

  EXPECT_CALL(local,
              SendChangesToSyncServer(UnorderedElementsAre(
                  SyncChangeAndDataMatch(
                      syncer::SyncChange::ACTION_UPDATE, "address-addr",
                      sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 5, 6),
                  SyncChangeAndDataMatch(
                      syncer::SyncChange::ACTION_UPDATE, "card-card",
                      sync_pb::WalletMetadataSpecifics::CARD, "card", 7, 8))));

  local.AutofillMultipleChanged();
}

// Verify that lower values of existing metadata are not sent to the sync server
// when multiple metadata values change at once.
TEST(AutofillWalletMetadataSyncableServiceTest,
     DontUpdateToLowerValueOnServerOnMultiChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  // These methods do not trigger notifications or sync:
  local.UpdateAddressStats(BuildAddress("addr", 0, 0));
  local.UpdateCardStats(BuildCard("card", 0, 0));

  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  local.AutofillMultipleChanged();
}

// Verify that erased local metadata is also erased from the sync server when
// multiple metadata values change at once.
TEST(AutofillWalletMetadataSyncableServiceTest, DeleteFromServerOnMultiChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  // This method dooes not trigger notifications or sync:
  local.ClearServerData();

  EXPECT_CALL(
      local,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, "address-addr"),
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, "card-card"))));

  local.AutofillMultipleChanged();
}

// Verify that empty sync change from the sync server does not trigger writing
// to disk or sending any data to the sync server.
TEST(AutofillWalletMetadataSyncableServiceTest, EmptySyncChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  local.ProcessSyncChanges(FROM_HERE, syncer::SyncChangeList());
}

syncer::SyncChange BuildChange(
    syncer::SyncChange::SyncChangeType change_type,
    const std::string& sync_tag,
    sync_pb::WalletMetadataSpecifics::Type metadata_type,
    const std::string& server_id,
    int64 use_count,
    int64 use_date) {
  sync_pb::EntitySpecifics entity;
  entity.mutable_wallet_metadata()->set_type(metadata_type);
  entity.mutable_wallet_metadata()->set_id(server_id);
  entity.mutable_wallet_metadata()->set_use_count(use_count);
  entity.mutable_wallet_metadata()->set_use_date(use_date);
  return syncer::SyncChange(
      FROM_HERE, change_type,
      syncer::SyncData::CreateLocalData(sync_tag, sync_tag, entity));
}

// Verify that new metadata from the sync server is ignored when processing
// on-going sync changes. There should be no disk writes or messages to the sync
// server.
TEST(AutofillWalletMetadataSyncableServiceTest,
     IgnoreNewMetadataFromServerOnSyncChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  syncer::SyncChangeList changes;
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_ADD, "address-new-addr",
                  sync_pb::WalletMetadataSpecifics::ADDRESS, "new-addr", 5, 6));
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_ADD, "card-new-card",
                                sync_pb::WalletMetadataSpecifics::CARD,
                                "new-card", 7, 8));

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  local.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that higher values of metadata from the sync server are saved to
// disk when processing on-going sync changes.
TEST(AutofillWalletMetadataSyncableServiceTest,
     SaveHigherValuesFromServerOnSyncChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  syncer::SyncChangeList changes;
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, "address-addr",
                  sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 10, 20));
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_UPDATE, "card-card",
                                sync_pb::WalletMetadataSpecifics::CARD, "card",
                                30, 40));

  EXPECT_CALL(local,
              UpdateAddressStats(AutofillMetadataMatches("addr", 10, 20)));
  EXPECT_CALL(local, UpdateCardStats(AutofillMetadataMatches("card", 30, 40)));
  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  local.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that higher local values of metadata are sent to the sync server when
// processing on-going sync changes.
TEST(AutofillWalletMetadataSyncableServiceTest,
     SendHigherValuesToServerOnSyncChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  syncer::SyncChangeList changes;
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, "address-addr",
                  sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 0, 0));
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_UPDATE, "card-card",
                                sync_pb::WalletMetadataSpecifics::CARD, "card",
                                0, 0));

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local,
              SendChangesToSyncServer(UnorderedElementsAre(
                  SyncChangeAndDataMatch(
                      syncer::SyncChange::ACTION_UPDATE, "address-addr",
                      sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 1, 2),
                  SyncChangeAndDataMatch(
                      syncer::SyncChange::ACTION_UPDATE, "card-card",
                      sync_pb::WalletMetadataSpecifics::CARD, "card", 3, 4))));

  local.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that addition of known metadata is treated the same as an update.
TEST(AutofillWalletMetadataSyncableServiceTest,
     TreatAdditionOfKnownMetadataAsUpdateOnSyncChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  syncer::SyncChangeList changes;
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_ADD, "address-addr",
                                sync_pb::WalletMetadataSpecifics::ADDRESS,
                                "addr", 0, 0));
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_ADD, "card-card",
                                sync_pb::WalletMetadataSpecifics::CARD, "card",
                                0, 0));

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local,
              SendChangesToSyncServer(UnorderedElementsAre(
                  SyncChangeAndDataMatch(
                      syncer::SyncChange::ACTION_UPDATE, "address-addr",
                      sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 1, 2),
                  SyncChangeAndDataMatch(
                      syncer::SyncChange::ACTION_UPDATE, "card-card",
                      sync_pb::WalletMetadataSpecifics::CARD, "card", 3, 4))));

  local.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that an update of locally unknown metadata is ignored. There should be
// no disk writes and no messages sent to the server.
TEST(AutofillWalletMetadataSyncableServiceTest,
     IgnoreUpdateOfUnknownMetadataOnSyncChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  syncer::SyncChangeList changes;
  changes.push_back(BuildChange(
      syncer::SyncChange::ACTION_UPDATE, "address-unknown-addr",
      sync_pb::WalletMetadataSpecifics::ADDRESS, "unknown-addr", 0, 0));
  changes.push_back(BuildChange(
      syncer::SyncChange::ACTION_UPDATE, "card-unknown-card",
      sync_pb::WalletMetadataSpecifics::CARD, "unknown-card", 0, 0));

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  local.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that deletion from the sync server of locally unknown metadata is
// ignored. There should be no disk writes and no messages sent to the server.
TEST(AutofillWalletMetadataSyncableServiceTest,
     IgnoreDeleteOfUnknownMetadataOnSyncChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  syncer::SyncChangeList changes;
  changes.push_back(BuildChange(
      syncer::SyncChange::ACTION_DELETE, "address-unknown-addr",
      sync_pb::WalletMetadataSpecifics::ADDRESS, "unknown-addr", 0, 0));
  changes.push_back(BuildChange(
      syncer::SyncChange::ACTION_DELETE, "card-unknown-card",
      sync_pb::WalletMetadataSpecifics::CARD, "unknown-card", 0, 0));

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local, SendChangesToSyncServer(_)).Times(0);

  local.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that deletion from the sync server of locally existing metadata will
// trigger an undelete message sent to the server.
TEST(AutofillWalletMetadataSyncableServiceTest,
     UndeleteExistingMetadataOnSyncChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr", 1, 2));
  local.UpdateCardStats(BuildCard("card", 3, 4));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr", 1, 2));
  remote.UpdateCardStats(BuildCard("card", 3, 4));
  MergeMetadata(&local, &remote);
  syncer::SyncChangeList changes;
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_DELETE, "address-addr",
                  sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 0, 0));
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_DELETE, "card-card",
                                sync_pb::WalletMetadataSpecifics::CARD, "card",
                                0, 0));

  EXPECT_CALL(local, UpdateAddressStats(_)).Times(0);
  EXPECT_CALL(local, UpdateCardStats(_)).Times(0);
  EXPECT_CALL(local,
              SendChangesToSyncServer(UnorderedElementsAre(
                  SyncChangeAndDataMatch(
                      syncer::SyncChange::ACTION_ADD, "address-addr",
                      sync_pb::WalletMetadataSpecifics::ADDRESS, "addr", 1, 2),
                  SyncChangeAndDataMatch(
                      syncer::SyncChange::ACTION_ADD, "card-card",
                      sync_pb::WalletMetadataSpecifics::CARD, "card", 3, 4))));

  local.ProcessSyncChanges(FROM_HERE, changes);
}

// Verify that processing sync changes maintains the local cache of sync server
// data, which is used to avoid calling the expensive GetAllSyncData() function.
TEST(AutofillWalletMetadataSyncableServiceTest,
     CacheIsUpToDateAfterSyncChange) {
  NiceMock<MockService> local;
  local.UpdateAddressStats(BuildAddress("addr1", 1, 2));
  local.UpdateAddressStats(BuildAddress("addr2", 3, 4));
  local.UpdateCardStats(BuildCard("card1", 5, 6));
  local.UpdateCardStats(BuildCard("card2", 7, 8));
  NiceMock<MockService> remote;
  remote.UpdateAddressStats(BuildAddress("addr1", 1, 2));
  remote.UpdateAddressStats(BuildAddress("addr2", 3, 4));
  remote.UpdateCardStats(BuildCard("card1", 5, 6));
  remote.UpdateCardStats(BuildCard("card2", 7, 8));
  MergeMetadata(&local, &remote);
  syncer::SyncChangeList changes;
  changes.push_back(
      BuildChange(syncer::SyncChange::ACTION_UPDATE, "address-addr1",
                  sync_pb::WalletMetadataSpecifics::ADDRESS, "addr1", 10, 20));
  changes.push_back(BuildChange(syncer::SyncChange::ACTION_UPDATE, "card-card1",
                                sync_pb::WalletMetadataSpecifics::CARD, "card1",
                                50, 60));
  local.ProcessSyncChanges(FROM_HERE, changes);
  // This method dooes not trigger notifications or sync:
  local.ClearServerData();

  EXPECT_CALL(
      local,
      SendChangesToSyncServer(UnorderedElementsAre(
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, "address-addr1"),
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, "address-addr2"),
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, "card-card1"),
          SyncChangeMatches(syncer::SyncChange::ACTION_DELETE, "card-card2"))));

  local.AutofillMultipleChanged();
}

}  // namespace
}  // namespace autofill
