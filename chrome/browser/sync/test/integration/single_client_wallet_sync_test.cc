// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/wallet_helper.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::AutofillMetadata;
using autofill::AutofillMetrics;
using autofill::AutofillProfile;
using autofill::CreditCard;
using autofill::CreditCardCloudTokenData;
using autofill::data_util::TruncateUTF8;
using base::ASCIIToUTF16;
using testing::Contains;
using wallet_helper::CreateDefaultSyncCreditCardCloudTokenData;
using wallet_helper::CreateDefaultSyncPaymentsCustomerData;
using wallet_helper::CreateDefaultSyncWalletAddress;
using wallet_helper::CreateDefaultSyncWalletCard;
using wallet_helper::CreateSyncCreditCardCloudTokenData;
using wallet_helper::CreateSyncPaymentsCustomerData;
using wallet_helper::CreateSyncWalletAddress;
using wallet_helper::CreateSyncWalletCard;
using wallet_helper::ExpectDefaultCreditCardValues;
using wallet_helper::ExpectDefaultProfileValues;
using wallet_helper::GetAccountWebDataService;
using wallet_helper::GetDefaultCreditCard;
using wallet_helper::GetPersonalDataManager;
using wallet_helper::GetProfileWebDataService;
using wallet_helper::GetServerAddressesMetadata;
using wallet_helper::GetServerCardsMetadata;
using wallet_helper::GetWalletDataModelTypeState;
using wallet_helper::kDefaultBillingAddressID;
using wallet_helper::kDefaultCardID;
using wallet_helper::kDefaultCreditCardCloudTokenDataID;
using wallet_helper::kDefaultCustomerID;
using wallet_helper::UnmaskServerCard;

namespace {

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

MATCHER(AddressHasConverted, "") {
  return arg.specifics().wallet_metadata().address_has_converted();
}

const char kLocalGuidA[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kDifferentBillingAddressId[] = "another address entity ID";
const base::Time kArbitraryDefaultTime = base::Time::FromDoubleT(25);
const syncer::SyncFirstSetupCompleteSource kSetSourceFromTest =
    syncer::SyncFirstSetupCompleteSource::BASIC_FLOW;

template <class T>
class AutofillWebDataServiceConsumer : public WebDataServiceConsumer {
 public:
  AutofillWebDataServiceConsumer() {}

  virtual ~AutofillWebDataServiceConsumer() {}

  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override {
    result_ = std::move(static_cast<WDResult<T>*>(result.get())->GetValue());
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  T& result() { return result_; }

 private:
  base::RunLoop run_loop_;
  T result_;
  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataServiceConsumer);
};

class WaitForWalletUpdateChecker : public StatusChangeChecker,
                                   public syncer::SyncServiceObserver {
 public:
  WaitForWalletUpdateChecker(base::Time min_required_progress_marker_timestamp,
                             syncer::SyncService* service)
      : min_required_progress_marker_timestamp_(
            min_required_progress_marker_timestamp),
        service_(service) {
    scoped_observer_.Add(service);
  }

  bool IsExitConditionSatisfied(std::ostream* os) override {
    // GetLastCycleSnapshot() returns by value, so make sure to capture it for
    // iterator use.
    const syncer::SyncCycleSnapshot snap =
        service_->GetLastCycleSnapshotForDebugging();
    const syncer::ProgressMarkerMap& progress_markers =
        snap.download_progress_markers();
    auto marker_it = progress_markers.find(syncer::AUTOFILL_WALLET_DATA);
    if (marker_it == progress_markers.end()) {
      *os << "Waiting for an updated Wallet progress marker timestamp "
          << min_required_progress_marker_timestamp_
          << "; actual: no progress marker in last sync cycle";
      return false;
    }

    sync_pb::DataTypeProgressMarker progress_marker;
    bool success = progress_marker.ParseFromString(marker_it->second);
    DCHECK(success);

    const base::Time actual_timestamp =
        fake_server::FakeServer::GetWalletProgressMarkerTimestamp(
            progress_marker);

    *os << "Waiting for an updated Wallet progress marker timestamp "
        << min_required_progress_marker_timestamp_ << "; actual "
        << actual_timestamp;

    return actual_timestamp >= min_required_progress_marker_timestamp_;
  }

  // syncer::SyncServiceObserver implementation.
  void OnSyncCycleCompleted(syncer::SyncService* sync) override {
    CheckExitCondition();
  }

 private:
  const base::Time min_required_progress_marker_timestamp_;
  const syncer::SyncService* const service_;

  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      scoped_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(WaitForWalletUpdateChecker);
};

std::vector<std::unique_ptr<CreditCard>> GetServerCards(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<CreditCard>>>
      consumer;
  service->GetServerCreditCards(&consumer);
  consumer.Wait();
  return std::move(consumer.result());
}

std::vector<std::unique_ptr<AutofillProfile>> GetServerProfiles(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer;
  service->GetServerProfiles(&consumer);
  consumer.Wait();
  return std::move(consumer.result());
}

#if !defined(OS_CHROMEOS)
std::unique_ptr<autofill::PaymentsCustomerData> GetPaymentsCustomerData(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  AutofillWebDataServiceConsumer<
      std::unique_ptr<autofill::PaymentsCustomerData>>
      consumer;
  service->GetPaymentsCustomerData(&consumer);
  consumer.Wait();
  return std::move(consumer.result());
}
#endif

std::vector<std::unique_ptr<autofill::CreditCardCloudTokenData>>
GetCreditCardCloudTokenData(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  AutofillWebDataServiceConsumer<
      std::vector<std::unique_ptr<CreditCardCloudTokenData>>>
      consumer;
  service->GetCreditCardCloudTokenData(&consumer);
  consumer.Wait();
  return std::move(consumer.result());
}

}  // namespace

class SingleClientWalletSyncTest : public SyncTest {
 public:
  SingleClientWalletSyncTest() : SyncTest(SINGLE_CLIENT) {
    test_clock_.SetNow(kArbitraryDefaultTime);
  }

  ~SingleClientWalletSyncTest() override {}

 protected:
  void WaitForOnPersonalDataChanged(autofill::PersonalDataManager* pdm) {
    pdm->AddObserver(&personal_data_observer_);
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillRepeatedly(QuitMessageLoop(&run_loop));
    run_loop.Run();
    pdm->RemoveObserver(&personal_data_observer_);
  }

  void WaitForNumberOfCards(size_t expected_count,
                            autofill::PersonalDataManager* pdm) {
    while (pdm->GetCreditCards().size() != expected_count ||
           pdm->HasPendingQueriesForTesting()) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  void WaitForNumberOfServerProfiles(size_t expected_count,
                                     autofill::PersonalDataManager* pdm) {
    while (pdm->GetServerProfiles().size() != expected_count ||
           pdm->HasPendingQueriesForTesting()) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  void WaitForPaymentsCustomerData(const std::string& customer_id,
                                   autofill::PersonalDataManager* pdm) {
    while (pdm->GetPaymentsCustomerData() == nullptr ||
           pdm->GetPaymentsCustomerData()->customer_id != customer_id ||
           pdm->HasPendingQueriesForTesting()) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  bool TriggerGetUpdatesAndWait() {
    const base::Time now = base::Time::Now();
    // Trigger a sync and wait for the new data to arrive.
    TriggerSyncForModelTypes(
        0, syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));
    return WaitForWalletUpdateChecker(now, GetSyncService(0)).Wait();
  }

  void AdvanceAutofillClockByOneDay() {
    test_clock_.Advance(base::TimeDelta::FromDays(1));
  }

  testing::NiceMock<PersonalDataLoadedObserverMock> personal_data_observer_;
  base::HistogramTester histogram_tester_;
  autofill::TestAutofillClock test_clock_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientWalletSyncTest);
};

class SingleClientWalletSyncTestWithoutAccountStorage
    : public SingleClientWalletSyncTest {
 public:
  SingleClientWalletSyncTestWithoutAccountStorage() {
    features_.InitAndDisableFeature(
        autofill::features::kAutofillEnableAccountWalletStorage);
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTestWithoutAccountStorage,
                       DownloadProfileStorage) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  auto profile_data = GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);
  auto account_data = GetAccountWebDataService(0);
  ASSERT_EQ(nullptr, account_data);

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());

  // Check that the data was set correctly.
  ExpectDefaultCreditCardValues(*cards[0]);
  ExpectDefaultProfileValues(*profiles[0]);

  // Check that the data is stored in the profile storage.
  EXPECT_EQ(1U, GetServerCards(GetProfileWebDataService(0)).size());
  EXPECT_EQ(1U, GetServerProfiles(GetProfileWebDataService(0)).size());
}

class SingleClientWalletWithAccountStorageSyncTest
    : public SingleClientWalletSyncTest {
 public:
  SingleClientWalletWithAccountStorageSyncTest() {
    features_.InitAndEnableFeature(
        autofill::features::kAutofillEnableAccountWalletStorage);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientWalletWithAccountStorageSyncTest,
                       DownloadAccountStorage_Card) {
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);

  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData()});

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  auto profile_data = GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);
  auto account_data = GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);

  // Check that no data is stored in the profile storage.
  EXPECT_EQ(0U, GetServerCards(profile_data).size());
  EXPECT_EQ(0U, GetServerProfiles(profile_data).size());

  // Check that one card is stored in the account storage.
  EXPECT_EQ(1U, GetServerCards(account_data).size());

  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());

  ExpectDefaultCreditCardValues(*cards[0]);

  // Now sign back out.
  GetClient(0)->SignOutPrimaryAccount();

  // Verify that sync is stopped.
  ASSERT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // Wait for PDM to receive the data change with no cards.
  WaitForNumberOfCards(0, pdm);

  // Check directly in the DB that the account storage is now cleared.
  EXPECT_EQ(0U, GetServerCards(account_data).size());
}

// Wallet data should get cleared from the database when the user signs out and
// different data should get downstreamed when the user signs in with a
// different account.
IN_PROC_BROWSER_TEST_F(SingleClientWalletWithAccountStorageSyncTest,
                       ClearOnSignOutAndDownstreamOnSignIn) {
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData(),
       CreateSyncCreditCardCloudTokenData(/*cloud_token_data_id=*/"data-1")});

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(AwaitQuiescence());

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());

  // Signout, the data & metadata should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());

  // Set a different set of cards on the server, then sign in again (this is a
  // good enough approximation of signing in with a different Google account).
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"new-card", /*last_four=*/"0002",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"new-address", /*company=*/"Company-2"),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"different"),
       CreateSyncCreditCardCloudTokenData(/*cloud_token_data_id=*/"data-2")});
  GetClient(0)->SignInPrimaryAccount();

  WaitForNumberOfCards(1, pdm);

  // Make sure the data is in the DB.
  EXPECT_EQ(ASCIIToUTF16("0002"), pdm->GetCreditCards()[0]->LastFourDigits());
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-2", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  ASSERT_EQ("different", pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ("data-2", pdm->GetCreditCardCloudTokenData()[0]->instrument_token);
}
#endif  // !defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, EnabledByDefault) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  // TODO(pvalenzuela): Assert that the local root node for AUTOFILL_WALLET_DATA
  // exists.
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_METADATA));
}

// Wallet data should get cleared from the database when sync is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnDisableSync) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());

  // Disable sync, the data & metadata should be gone.
  GetSyncService(0)->StopAndClear();
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());

  // Turn sync on again, the data should come back.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  // StopAndClear() also clears the "first setup complete" flag, so set it
  // again.
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);
  // Wait until Sync restores the card and it arrives at PDM.
  WaitForNumberOfCards(1, pdm);

  EXPECT_EQ(1uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(1uL, pdm->GetCreditCards().size());
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  EXPECT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(1U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(1U, GetServerAddressesMetadata(0).size());
}

// Wallet data should get cleared from the database when sync is (temporarily)
// stopped, e.g. due to the Sync feature toggle in Android settings.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnStopSync) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());

  // Stop sync, the data & metadata should be gone.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());

  // Turn sync on again, the data should come back.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  // Wait until Sync restores the card and it arrives at PDM.
  WaitForNumberOfCards(1, pdm);

  EXPECT_EQ(1uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(1uL, pdm->GetCreditCards().size());
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  EXPECT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(1U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(1U, GetServerAddressesMetadata(0).size());
}

// ChromeOS does not sign out, so the test below does not apply.
#if !defined(OS_CHROMEOS)
// Wallet data should get cleared from the database when the user signs out.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnSignOut) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());

  // Signout, the data & metadata should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());
}
#endif  // !defined(OS_CHROMEOS)

// Wallet is not using incremental updates. Make sure existing data gets
// replaced when synced down.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       NewSyncDataShouldReplaceExistingData) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID, /*nickname=*/""),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData(),
       CreateSyncCreditCardCloudTokenData(/*cloud_token_data_id=*/"data-1")});
  ASSERT_TRUE(SetupSync());

  // Make sure the data is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0001"), cards[0]->LastFourDigits());
  // When no nickname is returned from Sync server, credit card's nickname is
  // empty.
  EXPECT_TRUE(cards[0]->nickname().empty());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-1", cloud_token_data[0]->instrument_token);

  // Put some completely new data in the sync server.
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"new-card", /*last_four=*/"0002",
                            kDefaultBillingAddressID,
                            /*nickname=*/"Grocery Card"),
       CreateSyncWalletAddress(/*name=*/"new-address", /*company=*/"Company-2"),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"different"),
       CreateSyncCreditCardCloudTokenData(/*cloud_token_data_id=*/"data-2")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // Make sure only the new data is present.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0002"), cards[0]->LastFourDigits());
  EXPECT_EQ(ASCIIToUTF16("Grocery Card"), cards[0]->nickname());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-2", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  cloud_token_data = pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-2", cloud_token_data[0]->instrument_token);
}

// Wallet is not using incremental updates. The server either sends a non-empty
// update with deletion gc directives and with the (possibly empty) full data
// set, or (more often) an empty update.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, EmptyUpdatesAreIgnored) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData(),
       CreateSyncCreditCardCloudTokenData("data-1")});
  ASSERT_TRUE(SetupSync());

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0001"), cards[0]->LastFourDigits());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-1", cloud_token_data[0]->instrument_token);

  // Trigger a sync and wait for the new data to arrive.
  sync_pb::ModelTypeState state_before = GetWalletDataModelTypeState(0);
  ASSERT_TRUE(TriggerGetUpdatesAndWait());

  // Check that the new progress marker is stored for empty updates. This is a
  // regression check for crbug.com/924447.
  sync_pb::ModelTypeState state_after = GetWalletDataModelTypeState(0);
  EXPECT_NE(state_before.progress_marker().token(),
            state_after.progress_marker().token());

  // Refresh the pdm to make sure we are checking its state after any potential
  // changes from sync in the DB propagate into pdm. As we don't expect anything
  // to change, we have no better specific condition to wait for.
  pdm->Refresh();
  while (pdm->HasPendingQueriesForTesting()) {
    WaitForOnPersonalDataChanged(pdm);
  }

  // Make sure the same data is present on the client.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0001"), cards[0]->LastFourDigits());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  cloud_token_data = pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-1", cloud_token_data[0]->instrument_token);
}

// If the server sends the same cards and addresses again, they should not
// change on the client. We should also not overwrite existing metadata.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, SameUpdatesAreIgnored) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData(),
       CreateSyncCreditCardCloudTokenData("data-1")});
  ASSERT_TRUE(SetupSync());

  // Record use of to get non-default metadata values.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  pdm->RecordUseOf(*cards[0]);
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  pdm->RecordUseOf(*profiles[0]);

  // Keep the same data (only change the customer data and the cloud token to
  // force the FakeServer to send the full update).
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateSyncPaymentsCustomerData("different"),
       CreateSyncCreditCardCloudTokenData("data-2")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // Make sure the data is present on the client.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0001"), cards[0]->LastFourDigits());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-2", cloud_token_data[0]->instrument_token);

  // Test that the non-default metadata values stayed around.
  std::map<std::string, AutofillMetadata> cards_metadata =
      GetServerCardsMetadata(0);
  EXPECT_EQ(1U, cards_metadata.size());
  EXPECT_EQ(2U, cards_metadata.begin()->second.use_count);
  std::map<std::string, AutofillMetadata> addresses_metadata =
      GetServerAddressesMetadata(0);
  EXPECT_EQ(1U, addresses_metadata.size());
  // TODO(crbug.com/941498): Once RecordUseOf() is fixed for server addresses,
  // change this expectation back to 2.
  EXPECT_EQ(1U, addresses_metadata.begin()->second.use_count);
}

// If the server sends the same cards and addresses with changed data, they
// should change on the client.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ChangedEntityGetsUpdated) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0002",
                            kDefaultBillingAddressID, /*nickname=*/"Outdated"),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-2"),
       CreateDefaultSyncPaymentsCustomerData(),
       CreateSyncCreditCardCloudTokenData("data-1")});
  ASSERT_TRUE(SetupSync());

  // Record use of to get non-default metadata values.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  pdm->RecordUseOf(*cards[0]);
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  pdm->RecordUseOf(*profiles[0]);

  // Update the data (also change the customer data to force the full update as
  // FakeServer computes the hash for progress markers only based on ids). For
  // server card, only the card's nickname is changed, metadata is unchanged.
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0002",
                            kDefaultBillingAddressID,
                            /*nickname=*/"Grocery Card"),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateSyncPaymentsCustomerData("different"),
       CreateSyncCreditCardCloudTokenData("data-2")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // Make sure the data is present on the client.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("Grocery Card"), cards[0]->nickname());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-2", cloud_token_data[0]->instrument_token);

  // Test that the non-default metadata values stayed around.
  std::map<std::string, AutofillMetadata> cards_metadata =
      GetServerCardsMetadata(0);
  EXPECT_EQ(1U, cards_metadata.size());
  EXPECT_EQ(2U, cards_metadata.begin()->second.use_count);
  std::map<std::string, AutofillMetadata> addresses_metadata =
      GetServerAddressesMetadata(0);
  EXPECT_EQ(1U, addresses_metadata.size());
  // Metadata for server addresses cannot be preserved because the id changes
  // since it is generated on the client as a hash of all the data fields. As a
  // result, the metadata entry gets deleted and recreated under a different id
  // and has a default use count.
  EXPECT_EQ(1U, addresses_metadata.begin()->second.use_count);
}

// If the server sends the same cards again, they should not change on the
// client even if the cards on the client are unmasked.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       SameUpdatesAreIgnoredWhenLocalCardsUnmasked) {
// We need to allow storing full server cards for this test to work properly.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      autofill::switches::kEnableOfferStoreUnmaskedWalletCards);
#endif
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Check we have the card locally.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0001"), cards[0]->LastFourDigits());
  EXPECT_EQ(CreditCard::MASKED_SERVER_CARD, cards[0]->record_type());

  // Unmask the card (the full card number has to start with "34" to match the
  // type of the masked card which is by default AMEX in the tests).
  UnmaskServerCard(0, *cards[0], base::UTF8ToUTF16("3404000300020001"));

  // Keep the same data (only change the customer data to force the FakeServer
  // to send the full update).
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncPaymentsCustomerData("different")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // Make sure the data is present on the client.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0001"), cards[0]->LastFourDigits());
  EXPECT_EQ(CreditCard::FULL_SERVER_CARD, cards[0]->record_type());
  EXPECT_EQ("different", pdm->GetPaymentsCustomerData()->customer_id);
}

// Wallet data should get cleared from the database when the wallet sync type
// flag is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnDisableWalletSync) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());

  // Turn off autofill sync, the data & metadata should be gone.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kAutofill));

  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());
}

// Wallet data should get cleared from the database when the wallet autofill
// integration flag is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       ClearOnDisableWalletAutofill) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());

  // Turn off the wallet autofill pref, the data & metadata should be gone as a
  // side effect of the wallet model type controller noticing.
  autofill::prefs::SetPaymentsIntegrationEnabled(GetProfile(0)->GetPrefs(),
                                                 false);

  // It's not sufficient to wait for 0 cards (as in other tests) because
  // switching off the pref makes PDM (synchronously) return 0 cards (without
  // the other changes in the DB going through).
  WaitForNumberOfServerProfiles(0, pdm);

  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());
}

// Wallet data present on the client should be cleared in favor of the new data
// synced down form the server.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       NewWalletCardRemovesExistingCardAndProfile) {
  ASSERT_TRUE(SetupSync());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  std::vector<CreditCard> credit_cards = {credit_card};
  wallet_helper::SetServerCreditCards(0, credit_cards);

  // Add a server profile on the client.
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "a123");
  profile.SetRawInfo(autofill::COMPANY_NAME, ASCIIToUTF16("JustATest"));
  std::vector<AutofillProfile> client_profiles = {profile};
  wallet_helper::SetServerProfiles(0, client_profiles);

  // Add PaymentsCustomerData on the client.
  wallet_helper::SetPaymentsCustomerData(
      0, autofill::PaymentsCustomerData(/*customer_id=*/kDefaultCustomerID));

  // Add a cloud token data on the client.
  CreditCardCloudTokenData data;
  data.instrument_token = "data-1";
  wallet_helper::SetCreditCardCloudTokenData(0, {data});

  // Wait for the pdm to get data from the autofill table (the manual changes
  // above don't notify pdm to refresh automatically).
  pdm->Refresh();
  WaitForNumberOfCards(1, pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ("a123", cards[0]->server_id());

  // Make sure the profile was added correctly.
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("JustATest", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));

  // Make sure the customer data was added correctly.
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Make sure the cloud token data was added correctly.
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-1", cloud_token_data[0]->instrument_token);

  // Add new data to the server and sync it down.
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletCard(),
       CreateSyncPaymentsCustomerData("different"),
       CreateSyncCreditCardCloudTokenData("data-2")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // The only card present on the client should be the one from the server.
  cards = pdm->GetCreditCards();
  EXPECT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // There should be no profile present.
  profiles = pdm->GetServerProfiles();
  EXPECT_EQ(0uL, profiles.size());

  // The only cloud token present on the client should be the one from the
  // server.
  cloud_token_data = pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-2", cloud_token_data[0]->instrument_token);
}

// Wallet data present on the client should be cleared in favor of the new data
// synced down form the server.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       NewWalletDataRemovesExistingData) {
  ASSERT_TRUE(SetupSync());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server profile on the client.
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "a123");
  profile.SetRawInfo(autofill::COMPANY_NAME, ASCIIToUTF16("JustATest"));
  std::vector<AutofillProfile> client_profiles = {profile};
  wallet_helper::SetServerProfiles(0, client_profiles);

  // Add a server credit card on the client.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  std::vector<CreditCard> credit_cards = {credit_card};
  wallet_helper::SetServerCreditCards(0, credit_cards);

  // Add PaymentsCustomerData on the client.
  wallet_helper::SetPaymentsCustomerData(
      0, autofill::PaymentsCustomerData(/*customer_id=*/kDefaultCustomerID));

  // Add CreditCardCloudTokenData on the client.
  CreditCardCloudTokenData data;
  data.instrument_token = "data-1";
  wallet_helper::SetCreditCardCloudTokenData(0, {data});

  // Wait for the pdm to get data from the autofill table (the manual changes
  // above don't notify pdm to refresh automatically).
  pdm->Refresh();
  WaitForNumberOfCards(1, pdm);

  // Make sure the profile was added correctly.
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  EXPECT_EQ(1uL, profiles.size());
  EXPECT_EQ("JustATest", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  EXPECT_EQ(1uL, cards.size());
  EXPECT_EQ("a123", cards[0]->server_id());

  // Make sure the customer data was added correctly.
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Make sure the credit card cloud token data was added correctly.
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-1", cloud_token_data[0]->instrument_token);

  // Add a new profile and new customer data from the server and sync them down.
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"different"),
       CreateSyncCreditCardCloudTokenData("data-2")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // The only profile present on the client should be the one from the server.
  profiles = pdm->GetServerProfiles();
  EXPECT_EQ(1uL, profiles.size());
  EXPECT_NE("JustATest", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));

  // There should be no cards present.
  cards = pdm->GetCreditCards();
  EXPECT_EQ(0uL, cards.size());

  // Credit card cloud token data should be updated.
  cloud_token_data = pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-2", cloud_token_data[0]->instrument_token);
}

// Tests that a local billing address id set on a card on the client should not
// be overwritten when that same card is synced again.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       SameWalletCard_PreservesLocalBillingAddressId) {
  ASSERT_TRUE(SetupSync());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client but with the billing address id of a
  // local profile.
  CreditCard credit_card = GetDefaultCreditCard();
  credit_card.set_billing_address_id(kLocalGuidA);
  std::vector<CreditCard> credit_cards = {credit_card};
  wallet_helper::SetServerCreditCards(0, credit_cards);

  // Wait for the pdm to get data from the autofill table (the manual changes
  // above don't notify pdm to refresh automatically).
  pdm->Refresh();
  WaitForNumberOfCards(1, pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // Sync the same card from the server, except with a default billing address
  // id.
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletCard(),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"different")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // The billing address is should still refer to the local profile.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());
  EXPECT_EQ(kLocalGuidA, cards[0]->billing_address_id());
}

// Tests that a server billing address id set on a card on the client is
// overwritten when that same card is synced again.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       SameWalletCard_DiscardsOldServerBillingAddressId) {
  ASSERT_TRUE(SetupSync());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client but with the billing address id of a
  // server profile.
  CreditCard credit_card = GetDefaultCreditCard();
  credit_card.set_billing_address_id(kDifferentBillingAddressId);
  std::vector<CreditCard> credit_cards = {credit_card};
  wallet_helper::SetServerCreditCards(0, credit_cards);

  // Wait for the pdm to get data from the autofill table (the manual changes
  // above don't notify pdm to refresh automatically).
  pdm->Refresh();
  WaitForNumberOfCards(1, pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // Sync the same card from the server, except with a default billing address
  // id.
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletCard(),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"different")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // The billing address should be the one from the server card.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());
  EXPECT_EQ(kDefaultBillingAddressID, cards[0]->billing_address_id());
}

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ConvertServerAddress) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Wait to make sure the address got converted locally.
  AutofillWalletConversionChecker(0).Wait();

  // Make sure the wallet_metadata is committed to the server, make an
  // independent later commit and wait for it to happen.
  ASSERT_TRUE(
      bookmarks_helper::AddURL(0, "What are you syncing about?",
                               GURL("https://google.com/synced-bookmark-1")));
  ASSERT_TRUE(ServerCountMatchStatusChecker(syncer::BOOKMARKS, 1).Wait());

  // Make sure the data is present on the client.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Check also the data related to conversion to local profiles.
  ASSERT_EQ(1uL, pdm->GetProfiles().size());
  std::map<std::string, AutofillMetadata> addresses_metadata =
      GetServerAddressesMetadata(0);
  EXPECT_EQ(1U, addresses_metadata.size());
  EXPECT_TRUE(addresses_metadata.begin()->second.has_converted);

  // Check the data is correctly on the server.
  EXPECT_THAT(fake_server_->GetSyncEntitiesByModelType(
                  syncer::AUTOFILL_WALLET_METADATA),
              Contains(AddressHasConverted()));

  histogram_tester_.ExpectBucketCount(
      "Autofill.WalletAddressConversionType",
      /*bucket=*/AutofillMetrics::CONVERTED_ADDRESS_ADDED,
      /*count=*/1);
}

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       DoNotConvertServerAddressAgain) {
  sync_pb::SyncEntity address_entity =
      CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1");
  GetFakeServer()->SetWalletData(
      {address_entity, CreateDefaultSyncPaymentsCustomerData()});

  // Inject a wallet metadata entity that indicates the address has already been
  // converted.
  sync_pb::EntitySpecifics specifics;
  sync_pb::WalletMetadataSpecifics* wallet_metadata_specifics =
      specifics.mutable_wallet_metadata();
  wallet_metadata_specifics->set_type(
      sync_pb::WalletMetadataSpecifics::ADDRESS);
  wallet_metadata_specifics->set_use_count(3);
  wallet_metadata_specifics->set_use_date(10);
  wallet_metadata_specifics->set_address_has_converted(true);
  // The id gets generated on the client based on the data. Simulate the process
  // to get the same id. On top of it, we need to base64encode it.
  AutofillProfile address = autofill::ProfileFromSpecifics(
      address_entity.specifics().autofill_wallet().address());
  wallet_metadata_specifics->set_id(
      autofill::GetBase64EncodedId(address.server_id()));

  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name",
          /*client_tag_hash=*/"address-" + wallet_metadata_specifics->id(),
          specifics,
          /*creation_time=*/0,
          /*last_modified_time=*/0));

  ASSERT_TRUE(SetupSync());

  // Wait to make sure we receive the (already converted) metadata entity.
  AutofillWalletConversionChecker(0).Wait();

  // No conversion happens now.
  histogram_tester_.ExpectTotalCount("Autofill.WalletAddressConversionType",
                                     /*count=*/0);
}

class SingleClientWalletSecondaryAccountSyncTest
    : public SingleClientWalletSyncTest {
 public:
  SingleClientWalletSecondaryAccountSyncTest() {
    features_.InitAndEnableFeature(
        autofill::features::kAutofillEnableAccountWalletStorage);
  }
  ~SingleClientWalletSecondaryAccountSyncTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    test_signin_client_factory_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
  }

  void SetUpOnMainThread() override {
#if defined(OS_CHROMEOS)
    secondary_account_helper::InitNetwork();
#endif  // defined(OS_CHROMEOS)
    SyncTest::SetUpOnMainThread();
  }

  Profile* profile() { return GetProfile(0); }

 private:
  base::test::ScopedFeatureList features_;

  secondary_account_helper::ScopedSigninClientFactory
      test_signin_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientWalletSecondaryAccountSyncTest);
};

// ChromeOS doesn't support changes to the primary account after startup, so
// these secondary-account-related tests don't apply.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientWalletSecondaryAccountSyncTest,
                       SwitchesFromAccountToProfileStorageOnSyncOptIn) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});

  // Set up Sync in transport mode for a non-primary account.
  secondary_account_helper::SignInSecondaryAccount(
      profile(), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should use (ephemeral) account storage.
  EXPECT_FALSE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  auto account_data = GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);
  auto profile_data = GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);

  // Check that the data is stored in the account storage (ephemeral), but not
  // in the profile storage (persisted).
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(0U, GetServerCards(profile_data).size());
  EXPECT_NE(nullptr, GetPaymentsCustomerData(account_data).get());
  EXPECT_EQ(nullptr, GetPaymentsCustomerData(profile_data).get());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(profile_data).size());

  // Simulate the user opting in to full Sync: Make the account primary, and
  // set first-time setup to complete.
  secondary_account_helper::MakeAccountPrimary(profile(), "user@email.com");
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);

  // Wait for Sync to get reconfigured into feature mode.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched to persistent storage.
  EXPECT_TRUE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  // The data should now be in the profile storage (persisted).
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());
  EXPECT_EQ(nullptr, GetPaymentsCustomerData(account_data).get());
  EXPECT_NE(nullptr, GetPaymentsCustomerData(profile_data).get());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(profile_data).size());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientWalletSecondaryAccountSyncTest,
    SwitchesFromAccountToProfileStorageOnSyncOptInWithAdvancedSetup) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});

  // Set up Sync in transport mode for a non-primary account.
  secondary_account_helper::SignInSecondaryAccount(
      profile(), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should use (ephemeral) account storage.
  EXPECT_FALSE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  auto account_data = GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);
  auto profile_data = GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);

  // Check that the card is stored in the account storage (ephemeral), but not
  // in the profile storage (persisted).
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(0U, GetServerCards(profile_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(profile_data).size());

  // Simulate the user opting in to full Sync: First, make the account primary.
  secondary_account_helper::MakeAccountPrimary(profile(), "user@email.com");

  // Now start actually configuring Sync.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  auto setup_handle = GetSyncService(0)->GetSetupInProgressHandle();

  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kAutofill});

  // Once the user finishes the setup, the newly selected data types will
  // actually get configured.
  setup_handle.reset();
  ASSERT_EQ(syncer::SyncService::TransportState::CONFIGURING,
            GetSyncService(0)->GetTransportState());

  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);

  // Wait for Sync to get reconfigured into feature mode.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched to persistent storage.
  EXPECT_TRUE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  // The card should now be in the profile storage (persisted).
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(profile_data).size());
}
#endif  // !defined(OS_CHROMEOS)

// This tests that switching between Sync-the-feature and
// Sync-standalone-transport properly migrates server credit cards between the
// profile (i.e. persisted) and account (i.e. ephemeral) storage.
// Sync can either be turned off temporarily via
// SyncUserSettings::SetSyncRequested(false), or permanently via
// SyncService::StopAndClear. For full coverage, we test all transitions, and
// each time verify that the card is in the correct storage:
// 1. Start out in Sync-the-feature mode -> profile storage.
// 2. SetSyncRequested(false) -> account storage.
// 3. Enable Sync-the-feature again -> profile storage.
// 4. StopAndClear() -> account storage.
// 5. Enable Sync-the-feature again -> profile storage.
IN_PROC_BROWSER_TEST_F(SingleClientWalletWithAccountStorageSyncTest,
                       SwitchesBetweenAccountAndProfileStorageOnTogglingSync) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});

  // STEP 1. Set up Sync in full feature mode.
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should use the regular persisted (non-account) storage.
  EXPECT_TRUE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  auto account_data = GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);
  auto profile_data = GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);

  // Check that the card is stored in the profile storage (persisted), but not
  // in the account storage (ephemeral).
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(profile_data).size());

  // STEP 2. Turn off Sync-the-feature temporarily (e.g. the Sync feature toggle
  // on Android), i.e. leave the Sync data around.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);

  // Wait for Sync to get reconfigured into transport mode.
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched to ephemeral storage.
  EXPECT_FALSE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  // The card should now be in the account storage (ephemeral). Note that even
  // though we specified KEEP_DATA above, the card is *not* in the profile
  // storage (persisted) anymore, because AUTOFILL_WALLET_DATA is special-cased
  // to always clear its data when Sync is turned off.
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(0U, GetServerCards(profile_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(profile_data).size());

  // STEP 3. Turn Sync-the-feature on again.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);

  // Wait for Sync to get reconfigured into full feature mode again.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched back to persistent storage.
  EXPECT_TRUE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  // And the card should be in the profile i.e. persistent storage again.
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(profile_data).size());

  // STEP 4. Turn off Sync-the-feature again, but this time clear data.
  GetSyncService(0)->StopAndClear();

  // Wait for Sync to get reconfigured into transport mode.
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched to ephemeral storage.
  EXPECT_FALSE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  // The card should now be in the account storage (ephemeral).
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(0U, GetServerCards(profile_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(profile_data).size());

  // STEP 5. Turn Sync-the-feature on again.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);

  // Wait for Sync to get reconfigured into full feature mode again.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched back to persistent storage.
  EXPECT_TRUE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  // And the card should be in the profile i.e. persistent storage again.
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(profile_data).size());
}
