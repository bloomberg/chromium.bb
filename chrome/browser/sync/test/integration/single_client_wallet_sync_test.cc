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
using autofill_helper::GetPersonalDataManager;
using autofill_helper::GetProfileWebDataService;
using autofill_helper::GetAccountWebDataService;
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

void AddDefaultCard(fake_server::FakeServer* server) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      specifics.mutable_autofill_wallet();
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

  server->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromEntitySpecifics(
          kDefaultCardID, specifics, 12345, 12345));
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

void AddDefaultProfile(fake_server::FakeServer* server) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      specifics.mutable_autofill_wallet();
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

  server->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromEntitySpecifics(
          kDefaultAddressID, specifics, 12345, 12345));
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
  UssSwitchToggler() {
    if (GetParam()) {
      override_features_.InitAndEnableFeature(
          switches::kSyncUSSAutofillWalletData);
    } else {
      override_features_.InitAndDisableFeature(
          switches::kSyncUSSAutofillWalletData);
    }
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

}  // namespace

class SingleClientWalletSyncTest : public SyncTest {
 public:
  SingleClientWalletSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientWalletSyncTest() override {}

 protected:
  void RefreshAndWaitForOnPersonalDataChanged(
      autofill::PersonalDataManager* pdm) {
    pdm->AddObserver(&personal_data_observer_);
    pdm->Refresh();
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

// TODO: Merge the two fixtures into one when all tests are passing for USS.
class SingleClientWalletSyncTestIncludingUssTests
    : public UssSwitchToggler,
      public SingleClientWalletSyncTest {
 public:
  SingleClientWalletSyncTestIncludingUssTests(){};
  ~SingleClientWalletSyncTestIncludingUssTests() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientWalletSyncTestIncludingUssTests);
};

IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTestIncludingUssTests,
                       EnabledByDefault) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  // TODO(pvalenzuela): Assert that the local root node for AUTOFILL_WALLET_DATA
  // exists.
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_METADATA));
}

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, DownloadProfileStorage) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // Enabled.
      {},
      // Disabled.
      {autofill::features::kAutofillEnableAccountWalletStorage});
  AddDefaultProfile(GetFakeServer());
  AddDefaultCard(GetFakeServer());
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
// TODO(crbug.com/853688): Reenable once the USS implementation of
// AUTOFILL_WALLET_DATA (AutofillWalletSyncBridge) has sufficient functionality.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       DISABLED_DownloadAccountStorage_Card) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      // Enabled.
      {switches::kSyncStandaloneTransport, switches::kSyncUSSAutofillWalletData,
       autofill::features::kAutofillEnableAccountWalletStorage},
      // Disabled.
      {});

  ASSERT_TRUE(SetupClients());
  AddDefaultCard(GetFakeServer());
  AddDefaultProfile(GetFakeServer());

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

  // Check that one card and one profile are stored in the account storage.
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerProfiles(account_data).size());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());

  ExpectDefaultCreditCardValues(*cards[0]);
  ExpectDefaultProfileValues(*profiles[0]);
}
#endif  // !defined(OS_CHROMEOS)

// Wallet data should get cleared from the database when sync is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnDisableSync) {
  AddDefaultCard(GetFakeServer());
  AddDefaultProfile(GetFakeServer());
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

// Wallet data should get cleared from the database when the wallet sync type
// flag is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnDisableWalletSync) {
  AddDefaultCard(GetFakeServer());
  AddDefaultProfile(GetFakeServer());
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
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       ClearOnDisableWalletAutofill) {
  AddDefaultCard(GetFakeServer());
  AddDefaultProfile(GetFakeServer());
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
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       NewWalletCardRemovesExistingCardAndProfile) {
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
  AddDefaultCard(GetFakeServer());
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
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       NewWalletProfileRemovesExistingProfileAndCard) {
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
  AddDefaultProfile(GetFakeServer());
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
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       SameWalletCard_PreservesLocalBillingAddressId) {
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
  AddDefaultCard(GetFakeServer());
  ASSERT_TRUE(SetupSync());

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
  AddDefaultCard(GetFakeServer());
  ASSERT_TRUE(SetupSync());

  // The billing address should be the one from the server card.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());
  EXPECT_EQ(kDefaultBillingAddressId, cards[0]->billing_address_id());
}

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientWalletSyncTestIncludingUssTests,
                        ::testing::Values(false, true));
