// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/browser/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::AutofillProfile;
using autofill::CreditCard;
using autofill::data_util::TruncateUTF8;
using autofill_helper::GetAccountWebDataService;
using autofill_helper::GetPersonalDataManager;
using autofill_helper::GetProfileWebDataService;
using base::ASCIIToUTF16;

namespace {

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

// Constands for the credit card.
const char kDefaultCardID[] = "wallet card ID";
const int kDefaultCardExpMonth = 8;
const int kDefaultCardExpYear = 2087;
const char kDefaultCardLastFour[] = "1234";
const char kDefaultCardName[] = "Patrick Valenzuela";
const char kDefaultBillingAddressId[] = "billing address entity ID";
const sync_pb::WalletMaskedCreditCard_WalletCardType kDefaultCardType =
    sync_pb::WalletMaskedCreditCard::AMEX;

// Constants for the address.
const char kDefaultAddressID[] = "wallet address ID";
const char kDefaultAddressName[] = "John S. Doe";
const char kDefaultCompanyName[] = "The Company";
const char kDefaultStreetAddress[] = "1234 Fake Street\nApp 2";
const char kDefaultCity[] = "Cityville";
const char kDefaultState[] = "Stateful";
const char kDefaultCountry[] = "US";
const char kDefaultZip[] = "90011";
const char kDefaultPhone[] = "1.800.555.1234";
const char kDefaultSortingCode[] = "CEDEX";
const char kDefaultDependentLocality[] = "DepLoc";
const char kDefaultLanguageCode[] = "en";

const char kLocalGuidA[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kDifferentBillingAddressId[] = "another address entity ID";

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

class PersonalDataLoadedObserverMock
    : public autofill::PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() {}
  ~PersonalDataLoadedObserverMock() override {}

  MOCK_METHOD0(OnPersonalDataChanged, void());
};

class WaitForNextWalletUpdateChecker : public StatusChangeChecker,
                                       public syncer::SyncServiceObserver {
 public:
  explicit WaitForNextWalletUpdateChecker(
      browser_sync::ProfileSyncService* service)
      : service_(service),
        initial_marker_(GetInitialMarker(service)),
        scoped_observer_(this) {
    scoped_observer_.Add(service);
  }

  bool IsExitConditionSatisfied() override {
    // GetLastCycleSnapshot() returns by value, so make sure to capture it for
    // iterator use.
    const syncer::SyncCycleSnapshot snap = service_->GetLastCycleSnapshot();
    const syncer::ProgressMarkerMap& progress_markers =
        snap.download_progress_markers();
    auto marker_it = progress_markers.find(syncer::AUTOFILL_WALLET_DATA);
    if (marker_it == progress_markers.end()) {
      return false;
    }
    return marker_it->second != initial_marker_;
  }

  std::string GetDebugMessage() const override {
    return "Waiting for an updated Wallet progress marker.";
  }

  // syncer::SyncServiceObserver implementation.
  void OnSyncCycleCompleted(syncer::SyncService* sync) override {
    CheckExitCondition();
  }

 private:
  static std::string GetInitialMarker(
      const browser_sync::ProfileSyncService* service) {
    // GetLastCycleSnapshot() returns by value, so make sure to capture it for
    // iterator use.
    const syncer::SyncCycleSnapshot snap = service->GetLastCycleSnapshot();
    const syncer::ProgressMarkerMap& progress_markers =
        snap.download_progress_markers();
    auto marker_it = progress_markers.find(syncer::AUTOFILL_WALLET_DATA);
    if (marker_it == progress_markers.end()) {
      return "N/A";
    }
    return marker_it->second;
  }

  const browser_sync::ProfileSyncService* service_;
  const std::string initial_marker_;
  ScopedObserver<browser_sync::ProfileSyncService,
                 WaitForNextWalletUpdateChecker>
      scoped_observer_;
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

sync_pb::SyncEntity CreateDefaultSyncWalletCard() {
  sync_pb::SyncEntity entity;
  entity.set_name(kDefaultCardID);
  entity.set_id_string(kDefaultCardID);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(
      sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* credit_card =
      wallet_specifics->mutable_masked_card();
  credit_card->set_id(kDefaultCardID);
  credit_card->set_exp_month(kDefaultCardExpMonth);
  credit_card->set_exp_year(kDefaultCardExpYear);
  credit_card->set_last_four(kDefaultCardLastFour);
  credit_card->set_name_on_card(kDefaultCardName);
  credit_card->set_status(sync_pb::WalletMaskedCreditCard::VALID);
  credit_card->set_type(kDefaultCardType);
  credit_card->set_billing_address_id(kDefaultBillingAddressId);
  return entity;
}

sync_pb::SyncEntity CreateSyncWalletCard(const std::string& name,
                                         const std::string& last_four) {
  sync_pb::SyncEntity result = CreateDefaultSyncWalletCard();
  result.set_name(name);
  result.set_id_string(name);
  result.mutable_specifics()
      ->mutable_autofill_wallet()
      ->mutable_masked_card()
      ->set_last_four(last_four);
  return result;
}

CreditCard GetDefaultCreditCard() {
  CreditCard card(CreditCard::MASKED_SERVER_CARD, kDefaultCardID);
  card.SetExpirationMonth(kDefaultCardExpMonth);
  card.SetExpirationYear(kDefaultCardExpYear);
  card.SetNumber(base::UTF8ToUTF16(kDefaultCardLastFour));
  card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                  base::UTF8ToUTF16(kDefaultCardName));
  card.SetServerStatus(CreditCard::OK);
  card.SetNetworkForMaskedCard(autofill::kAmericanExpressCard);
  card.set_card_type(CreditCard::CARD_TYPE_CREDIT);
  card.set_billing_address_id(kDefaultBillingAddressId);
  return card;
}

sync_pb::SyncEntity CreateDefaultSyncWalletAddress() {
  sync_pb::SyncEntity entity;
  entity.set_name(kDefaultAddressID);
  entity.set_id_string(kDefaultAddressID);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);

  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS);

  sync_pb::WalletPostalAddress* wallet_address =
      wallet_specifics->mutable_address();
  wallet_address->set_id(kDefaultAddressID);
  wallet_address->set_recipient_name(kDefaultAddressName);
  wallet_address->set_company_name(kDefaultCompanyName);
  wallet_address->add_street_address(kDefaultStreetAddress);
  wallet_address->set_address_1(kDefaultState);
  wallet_address->set_address_2(kDefaultCity);
  wallet_address->set_address_3(kDefaultDependentLocality);
  wallet_address->set_postal_code(kDefaultZip);
  wallet_address->set_country_code(kDefaultCountry);
  wallet_address->set_phone_number(kDefaultPhone);
  wallet_address->set_sorting_code(kDefaultSortingCode);
  wallet_address->set_language_code(kDefaultLanguageCode);

  return entity;
}

sync_pb::SyncEntity CreateSyncWalletAddress(const std::string& name,
                                            const std::string& company) {
  sync_pb::SyncEntity result = CreateDefaultSyncWalletAddress();
  result.set_name(name);
  result.set_id_string(name);
  result.mutable_specifics()
      ->mutable_autofill_wallet()
      ->mutable_address()
      ->set_company_name(company);
  return result;
}

// TODO(sebsg): Instead add a function to create a card, and one to inject in
// the server. Then compare the cards directly.
void ExpectDefaultCreditCardValues(const CreditCard& card) {
  EXPECT_EQ(CreditCard::MASKED_SERVER_CARD, card.record_type());
  EXPECT_EQ(kDefaultCardID, card.server_id());
  EXPECT_EQ(base::UTF8ToUTF16(kDefaultCardLastFour), card.LastFourDigits());
  EXPECT_EQ(autofill::kAmericanExpressCard, card.network());
  EXPECT_EQ(kDefaultCardExpMonth, card.expiration_month());
  EXPECT_EQ(kDefaultCardExpYear, card.expiration_year());
  EXPECT_EQ(base::UTF8ToUTF16(kDefaultCardName),
            card.GetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(kDefaultBillingAddressId, card.billing_address_id());
}

// TODO(sebsg): Instead add a function to create a profile, and one to inject in
// the server. Then compare the profiles directly.
void ExpectDefaultProfileValues(const AutofillProfile& profile) {
  EXPECT_EQ(kDefaultLanguageCode, profile.language_code());
  EXPECT_EQ(
      kDefaultAddressName,
      TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(autofill::NAME_FULL))));
  EXPECT_EQ(kDefaultCompanyName,
            TruncateUTF8(
                base::UTF16ToUTF8(profile.GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultStreetAddress,
            TruncateUTF8(base::UTF16ToUTF8(
                profile.GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS))));
  EXPECT_EQ(kDefaultCity, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                              autofill::ADDRESS_HOME_CITY))));
  EXPECT_EQ(kDefaultState, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                               autofill::ADDRESS_HOME_STATE))));
  EXPECT_EQ(kDefaultZip, TruncateUTF8(base::UTF16ToUTF8(
                             profile.GetRawInfo(autofill::ADDRESS_HOME_ZIP))));
  EXPECT_EQ(kDefaultCountry, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                                 autofill::ADDRESS_HOME_COUNTRY))));
  EXPECT_EQ(kDefaultPhone, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                               autofill::PHONE_HOME_WHOLE_NUMBER))));
  EXPECT_EQ(kDefaultSortingCode,
            TruncateUTF8(base::UTF16ToUTF8(
                profile.GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE))));
  EXPECT_EQ(kDefaultDependentLocality,
            TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                autofill::ADDRESS_HOME_DEPENDENT_LOCALITY))));
}

// Class that enables or disables USS based on test parameter. Must be the first
// base class of the test fixture.
// TODO(jkrcal): When the new implementation fully launches, remove this class,
// convert all tests from *_P back to *_F and remove the instance at the end.
class UssSwitchToggler : public testing::WithParamInterface<bool> {
 public:
  UssSwitchToggler() {}

  // Sets up feature overrides, based on the parameter of the test.
  void InitWithDefaultFeatures() { InitWithFeatures({}, {}); }

  // Sets up feature overrides, adds the toggled feature on top of specified
  // |enabled_features| and |disabled_features|. Vectors are passed by value
  // because we need to alter them anyway.
  void InitWithFeatures(std::vector<base::Feature> enabled_features,
                        std::vector<base::Feature> disabled_features) {
    if (GetParam()) {
      enabled_features.push_back(switches::kSyncUSSAutofillWalletData);
    } else {
      disabled_features.push_back(switches::kSyncUSSAutofillWalletData);
    }

    override_features_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

}  // namespace

class SingleClientWalletSyncTest : public UssSwitchToggler, public SyncTest {
 public:
  SingleClientWalletSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientWalletSyncTest() override {}

 protected:
  void RefreshAndWaitForOnPersonalDataChanged(
      autofill::PersonalDataManager* pdm) {
    WaitForOnPersonalDataChanged(/*should_trigger_refresh=*/true, pdm);
  }

  void WaitForOnPersonalDataChanged(bool should_trigger_refresh,
                                    autofill::PersonalDataManager* pdm) {
    pdm->AddObserver(&personal_data_observer_);
    if (should_trigger_refresh) {
      pdm->Refresh();
    }
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillRepeatedly(QuitMessageLoop(&run_loop));
    run_loop.Run();
    pdm->RemoveObserver(&personal_data_observer_);
  }

  PersonalDataLoadedObserverMock personal_data_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientWalletSyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, EnabledByDefault) {
  InitWithDefaultFeatures();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  // TODO(pvalenzuela): Assert that the local root node for AUTOFILL_WALLET_DATA
  // exists.
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_METADATA));
}

IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, DownloadProfileStorage) {
  base::test::ScopedFeatureList feature_list;
  InitWithFeatures(/*enabled_features=*/{},
                   /*disabled_features=*/
                   {autofill::features::kAutofillEnableAccountWalletStorage});
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(), CreateDefaultSyncWalletCard()});
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

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !defined(OS_CHROMEOS)
// The account storage requires USS, so we only test the USS implementation
// here.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       DownloadAccountStorage_Card) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{switches::kSyncStandaloneTransport,
                            switches::kSyncUSSAutofillWalletData,
                            autofill::features::
                                kAutofillEnableAccountWalletStorage},
      /*disabled_features=*/{});

  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  pdm->OnSyncServiceInitialized(GetSyncService(0));

  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(), CreateDefaultSyncWalletCard()});

  ASSERT_TRUE(GetClient(0)->SignIn());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization(
      /*skip_passphrase_verification=*/false));
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
  GetClient(0)->SignoutSyncService();

  // Verify that sync is stopped.
  ASSERT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // Wait for PDM to receive the data change.
  WaitForOnPersonalDataChanged(/*should_trigger_refresh=*/false, pdm);

  // Check that the account storage is now cleared.
  EXPECT_EQ(0U, GetServerCards(account_data).size());

  cards = pdm->GetCreditCards();
  EXPECT_EQ(0U, cards.size());
}
#endif  // !defined(OS_CHROMEOS)

// Wallet data should get cleared from the database when sync is disabled.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, ClearOnDisableSync) {
  InitWithDefaultFeatures();
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(), CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());

  // Turn off sync, the card should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSyncForAllDatatypes());
  cards = pdm->GetCreditCards();
  ASSERT_EQ(0uL, cards.size());
}

// ChromeOS does not sign out, so the test below does not apply.
#if !defined(OS_CHROMEOS)
// Wallet data should get cleared from the database when the user signs out.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, ClearOnSignOut) {
  InitWithDefaultFeatures();
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(), CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());

  // Turn off sync, the card should be gone.
  GetClient(0)->SignoutSyncService();

  ASSERT_EQ(0uL, pdm->GetCreditCards().size());
}
#endif  // !defined(OS_CHROMEOS)

// Wallet is not using incremental updates. Make sure existing data gets
// replaced when synced down.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       NewSyncDataShouldReplaceExistingData) {
  InitWithDefaultFeatures();
  sync_pb::SyncEntity first_card =
      CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001");
  sync_pb::SyncEntity first_address =
      CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1");

  GetFakeServer()->SetWalletData({first_card, first_address});
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

  sync_pb::SyncEntity new_card =
      CreateSyncWalletCard(/*name=*/"new-card", /*last_four=*/"0002");
  sync_pb::SyncEntity new_address =
      CreateSyncWalletAddress(/*name=*/"new-address", /*company=*/"Company-2");

  GetFakeServer()->SetWalletData({new_card, new_address});

  // Constructing the checker captures the current progress marker. Make sure to
  // do that before triggering the fetch.
  WaitForNextWalletUpdateChecker checker(GetSyncService(0));
  // Trigger a sync and wait for the new data to arrive.
  TriggerSyncForModelTypes(0,
                           syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));
  ASSERT_TRUE(checker.Wait());

  // Make sure only the new data is present.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0002"), cards[0]->LastFourDigits());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-2", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
}

// Wallet is not using incremental updates. The server either sends a non-empty
// update with deletion gc directives and with the (possibly empty) full data
// set, or (more often) an empty update.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, EmptyUpdatesAreIgnored) {
  InitWithDefaultFeatures();
  sync_pb::SyncEntity first_card =
      CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001");
  sync_pb::SyncEntity first_address =
      CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1");

  GetFakeServer()->SetWalletData({first_card, first_address});
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

  // Do not change anything on the server so that the update forced below is an
  // empty one.

  // Constructing the checker captures the current progress marker. Make sure to
  // do that before triggering the fetch.
  WaitForNextWalletUpdateChecker checker(GetSyncService(0));
  // Trigger a sync and wait for the new data to arrive.
  TriggerSyncForModelTypes(0,
                           syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));
  ASSERT_TRUE(checker.Wait());

  // Make sure the same data is present on the client.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0001"), cards[0]->LastFourDigits());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
}

// Wallet data should get cleared from the database when the wallet sync type
// flag is disabled.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, ClearOnDisableWalletSync) {
  InitWithDefaultFeatures();
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(), CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());

  // Turn off autofill sync, the card should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(syncer::AUTOFILL));
  cards = pdm->GetCreditCards();
  ASSERT_EQ(0uL, cards.size());
}

// Wallet data should get cleared from the database when the wallet autofill
// integration flag is disabled.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       ClearOnDisableWalletAutofill) {
  InitWithDefaultFeatures();
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(), CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());

  // Turn off the wallet autofill pref, the card should be gone as a side
  // effect of the wallet data type controller noticing.
  autofill::prefs::SetPaymentsIntegrationEnabled(GetProfile(0)->GetPrefs(),
                                                 false);
  cards = pdm->GetCreditCards();
  ASSERT_EQ(0uL, cards.size());
}

// Wallet data present on the client should be cleared in favor of the new data
// synced down form the server.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       NewWalletCardRemovesExistingCardAndProfile) {
  InitWithDefaultFeatures();
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  std::vector<CreditCard> credit_cards = {credit_card};
  autofill_helper::SetServerCreditCards(0, credit_cards);

  // Add a server profile on the client.
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "a123");
  profile.SetRawInfo(autofill::COMPANY_NAME, ASCIIToUTF16("JustATest"));
  std::vector<AutofillProfile> client_profiles = {profile};
  autofill_helper::SetServerProfiles(0, client_profiles);

  // Refresh the pdm so that it gets cards from autofill table.
  RefreshAndWaitForOnPersonalDataChanged(pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ("a123", cards[0]->server_id());

  // Make sure the profile was added correctly.
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("JustATest", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));

  // Add a new card from the server and sync it down.
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  // The only card present on the client should be the one from the server.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // There should be no profile present.
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(0uL, profiles.size());
}

// Wallet data present on the client should be cleared in favor of the new data
// synced down form the server.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       NewWalletProfileRemovesExistingProfileAndCard) {
  InitWithDefaultFeatures();
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server profile on the client.
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "a123");
  profile.SetRawInfo(autofill::COMPANY_NAME, ASCIIToUTF16("JustATest"));
  std::vector<AutofillProfile> client_profiles = {profile};
  autofill_helper::SetServerProfiles(0, client_profiles);

  // Add a server credit card on the client.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  std::vector<CreditCard> credit_cards = {credit_card};
  autofill_helper::SetServerCreditCards(0, credit_cards);

  // Refresh the pdm so that it gets cards from autofill table.
  RefreshAndWaitForOnPersonalDataChanged(pdm);

  // Make sure the profile was added correctly.
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("JustATest", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ("a123", cards[0]->server_id());

  // Add a new profile from the server and sync it down.
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress()});
  ASSERT_TRUE(SetupSync());

  // The only profile present on the client should be the one from the server.
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ(kDefaultCompanyName,
            TruncateUTF8(base::UTF16ToUTF8(
                profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));

  // There should be not card present.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(0uL, cards.size());
}

// Tests that a local billing address id set on a card on the client should not
// be overwritten when that same card is synced again.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       SameWalletCard_PreservesLocalBillingAddressId) {
  InitWithDefaultFeatures();
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client but with the billing address id of a
  // local profile.
  CreditCard credit_card = GetDefaultCreditCard();
  credit_card.set_billing_address_id(kLocalGuidA);
  std::vector<CreditCard> credit_cards = {credit_card};
  autofill_helper::SetServerCreditCards(0, credit_cards);

  // Refresh the pdm so that it gets cards from autofill table.
  RefreshAndWaitForOnPersonalDataChanged(pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // Sync the same card from the server, except with a default billing address
  // id.
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  // The billing address is should still refer to the local profile.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());
  EXPECT_EQ(kLocalGuidA, cards[0]->billing_address_id());
}

// Tests that a server billing address id set on a card on the client is
// overwritten when that same card is synced again.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       SameWalletCard_DiscardsOldServerBillingAddressId) {
  InitWithDefaultFeatures();
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client but with the billing address id of a
  // server profile.
  CreditCard credit_card = GetDefaultCreditCard();
  credit_card.set_billing_address_id(kDifferentBillingAddressId);
  std::vector<CreditCard> credit_cards = {credit_card};
  autofill_helper::SetServerCreditCards(0, credit_cards);

  // Refresh the pdm so that it gets cards from autofill table.
  RefreshAndWaitForOnPersonalDataChanged(pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // Sync the same card from the server, except with a default billing address
  // id.
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  // The billing address should be the one from the server card.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());
  EXPECT_EQ(kDefaultBillingAddressId, cards[0]->billing_address_id());
}

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientWalletSyncTest,
                        ::testing::Values(false, true));
