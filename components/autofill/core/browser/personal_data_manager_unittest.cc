// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {
namespace {

enum UserMode { USER_MODE_NORMAL, USER_MODE_INCOGNITO };

ACTION(QuitMainMessageLoop) {
  base::MessageLoop::current()->QuitWhenIdle();
}

class PersonalDataLoadedObserverMock : public PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() {}
  virtual ~PersonalDataLoadedObserverMock() {}

  MOCK_METHOD0(OnPersonalDataChanged, void());
};

template <typename T>
bool CompareElements(T* a, T* b) {
  return a->Compare(*b) < 0;
}

template <typename T>
bool ElementsEqual(T* a, T* b) {
  return a->Compare(*b) == 0;
}

// Verifies that two vectors have the same elements (according to T::Compare)
// while ignoring order. This is useful because multiple profiles or credit
// cards that are added to the SQLite DB within the same second will be returned
// in GUID (aka random) order.
template <typename T>
void ExpectSameElements(const std::vector<T*>& expectations,
                        const std::vector<T*>& results) {
  ASSERT_EQ(expectations.size(), results.size());

  std::vector<T*> expectations_copy = expectations;
  std::sort(
      expectations_copy.begin(), expectations_copy.end(), CompareElements<T>);
  std::vector<T*> results_copy = results;
  std::sort(results_copy.begin(), results_copy.end(), CompareElements<T>);

  EXPECT_EQ(std::mismatch(results_copy.begin(),
                          results_copy.end(),
                          expectations_copy.begin(),
                          ElementsEqual<T>).first,
            results_copy.end());
}

}  // anonymous namespace

class PersonalDataManagerTest : public testing::Test {
 protected:
  PersonalDataManagerTest() : autofill_table_(nullptr) {}

  void SetUp() override {
    prefs_ = test::PrefServiceForTesting();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = temp_dir_.path().AppendASCII("TestWebDB");
    web_database_ =
        new WebDatabaseService(path, base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get());

    // Setup account tracker.
    signin_client_.reset(new TestSigninClient(prefs_.get()));
    account_tracker_.reset(new AccountTrackerService());
    account_tracker_->Initialize(signin_client_.get());
    signin_manager_.reset(new FakeSigninManagerBase(signin_client_.get(),
                                                    account_tracker_.get()));
    signin_manager_->Initialize(prefs_.get());

    // Hacky: hold onto a pointer but pass ownership.
    autofill_table_ = new AutofillTable;
    web_database_->AddTable(scoped_ptr<WebDatabaseTable>(autofill_table_));
    web_database_->LoadDatabase();
    autofill_database_service_ = new AutofillWebDataService(
        web_database_, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(),
        WebDataServiceBase::ProfileErrorCallback());
    autofill_database_service_->Init();

    test::DisableSystemServices(prefs_.get());
    ResetPersonalDataManager(USER_MODE_NORMAL);

    // There are no field trials enabled by default.
    field_trial_list_.reset();
  }

  void TearDown() override {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    signin_manager_->Shutdown();
    signin_manager_.reset();

    account_tracker_->Shutdown();
    account_tracker_.reset();
    signin_client_.reset();
  }

  void ResetPersonalDataManager(UserMode user_mode) {
    bool is_incognito = (user_mode == USER_MODE_INCOGNITO);
    personal_data_.reset(new PersonalDataManager("en"));
    personal_data_->Init(
        scoped_refptr<AutofillWebDataService>(autofill_database_service_),
        prefs_.get(),
        account_tracker_.get(),
        signin_manager_.get(),
        is_incognito);
    personal_data_->AddObserver(&personal_data_observer_);

    // Verify that the web database has been updated and the notification sent.
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillOnce(QuitMainMessageLoop());
    base::MessageLoop::current()->Run();
  }

  void ResetProfiles() {
    std::vector<AutofillProfile> empty_profiles;
    personal_data_->SetProfiles(&empty_profiles);
  }

  void EnableWalletCardImport() {
    prefs_->SetBoolean(prefs::kAutofillWalletSyncExperimentEnabled, true);
    signin_manager_->SetAuthenticatedAccountInfo("12345",
                                                 "syncuser@example.com");
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableOfferStoreUnmaskedWalletCards);
  }

  void SetupReferenceProfile() {
    ASSERT_EQ(0U, personal_data_->GetProfiles().size());

    AutofillProfile profile(base::GenerateGUID(), "https://www.example.com");
    test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                         "johnwayne@me.xyz", "Fox", "123 Zoo St", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910");
    personal_data_->AddProfile(profile);

    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillOnce(QuitMainMessageLoop());
    base::MessageLoop::current()->Run();

    ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  }

  // Helper method that will add credit card fields in |form|, according to the
  // specified values. If a value is nullptr, the corresponding field won't get
  // added (empty string will add a field with an empty string as the value).
  void AddFullCreditCardForm(FormData* form,
                             const char* name,
                             const char* number,
                             const char* month,
                             const char* year) {
    FormFieldData field;
    if (name) {
      test::CreateTestFormField("Name on card:", "name_on_card", name, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (number) {
      test::CreateTestFormField("Card Number:", "card_number", number, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (month) {
      test::CreateTestFormField("Exp Month:", "exp_month", month, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (year) {
      test::CreateTestFormField("Exp Year:", "exp_year", year, "text", &field);
      form->fields.push_back(field);
    }
  }

  // Adds three local cards to the |personal_data_|. The three cards are
  // different: two are from different companies and the third doesn't have a
  // number. All three have different owners and credit card number. This allows
  // to test the suggestions based on name as well as on credit card number.
  void SetupReferenceLocalCreditCards() {
    ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

    CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                            "https://www.example.com");
    test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                            "347666888555" /* American Express */, "04",
                            "2015");
    credit_card0.set_use_count(3);
    credit_card0.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(1));
    personal_data_->AddCreditCard(credit_card0);

    CreditCard credit_card1("1141084B-72D7-4B73-90CF-3D6AC154673B",
                            "https://www.example.com");
    credit_card1.set_use_count(300);
    credit_card1.set_use_date(base::Time::Now() -
                              base::TimeDelta::FromDays(10));
    test::SetCreditCardInfo(&credit_card1, "John Dillinger",
                            "423456789012" /* Visa */, "01", "2010");
    personal_data_->AddCreditCard(credit_card1);

    CreditCard credit_card2("002149C1-EE28-4213-A3B9-DA243FFF021B",
                            "https://www.example.com");
    credit_card2.set_use_count(1);
    credit_card2.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(1));
    test::SetCreditCardInfo(&credit_card2, "Bonnie Parker",
                            "518765432109" /* Mastercard */, "", "");
    personal_data_->AddCreditCard(credit_card2);

    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillOnce(QuitMainMessageLoop());
    base::MessageLoop::current()->Run();

    ASSERT_EQ(3U, personal_data_->GetCreditCards().size());
  }

  // Helper methods that simply forward the call to the private member (to avoid
  // having to friend every test that needs to access the private
  // PersonalDataManager::ImportAddressProfile or ImportCreditCard).
  bool ImportAddressProfile(const FormStructure& form) {
    return personal_data_->ImportAddressProfile(form);
  }
  bool ImportCreditCard(const FormStructure& form,
                        bool should_return_local_card,
                        scoped_ptr<CreditCard>* imported_credit_card) {
    return personal_data_->ImportCreditCard(form, should_return_local_card,
                                            imported_credit_card);
  }

  // Sets up the profile order field trial group and parameter. Sets up the
  // suggestions limit parameter to |limit_param|.
  void EnableAutofillProfileLimitFieldTrial(const std::string& limit_param) {
    DCHECK(!limit_param.empty());

    // Clear the existing |field_trial_list_| and variation parameters.
    field_trial_list_.reset(NULL);
    field_trial_list_.reset(
        new base::FieldTrialList(new metrics::SHA1EntropyProvider("foo")));
    variations::testing::ClearAllVariationParams();

    std::map<std::string, std::string> params;
    params[kFrecencyFieldTrialLimitParam] = limit_param;
    variations::AssociateVariationParams(kFrecencyFieldTrialName, "LimitToN",
                                         params);

    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        kFrecencyFieldTrialName, "LimitToN");
    field_trial_->group();
  }

  // The temporary directory should be deleted at the end to ensure that
  // files are not used anymore and deletion succeeds.
  base::ScopedTempDir temp_dir_;
  base::MessageLoopForUI message_loop_;
  scoped_ptr<PrefService> prefs_;
  scoped_ptr<AccountTrackerService> account_tracker_;
  scoped_ptr<FakeSigninManagerBase> signin_manager_;
  scoped_ptr<TestSigninClient> signin_client_;
  scoped_refptr<AutofillWebDataService> autofill_database_service_;
  scoped_refptr<WebDatabaseService> web_database_;
  AutofillTable* autofill_table_;  // weak ref
  PersonalDataLoadedObserverMock personal_data_observer_;
  scoped_ptr<PersonalDataManager> personal_data_;

  scoped_ptr<base::FieldTrialList> field_trial_list_;
  scoped_refptr<base::FieldTrial> field_trial_;
};

TEST_F(PersonalDataManagerTest, AddProfile) {
  // Add profile0 to the database.
  AutofillProfile profile0(test::GetFullProfile());
  profile0.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("j@s.com"));
  personal_data_->AddProfile(profile0);

  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify the addition.
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, profile0.Compare(*results1[0]));

  // Add profile with identical values.  Duplicates should not get saved.
  AutofillProfile profile0a = profile0;
  profile0a.set_guid(base::GenerateGUID());
  personal_data_->AddProfile(profile0a);

  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify the non-addition.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // New profile with different email.
  AutofillProfile profile1 = profile0;
  profile1.set_guid(base::GenerateGUID());
  profile1.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("john@smith.com"));

  // Add the different profile.  This should save as a separate profile.
  // Note that if this same profile was "merged" it would collapse to one
  // profile with a multi-valued entry for email.
  personal_data_->AddProfile(profile1);

  // Reload the database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify the addition.
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());
}

TEST_F(PersonalDataManagerTest, DontDuplicateServerProfile) {
  EnableWalletCardImport();

  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, "a123"));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "",
                       "ACME Corp", "500 Oak View", "Apt 8", "Houston", "TX",
                       "77401", "US", "");
  // Wallet only provides a full name, so the above first and last names
  // will be ignored when the profile is written to the DB.
  server_profiles.back().SetRawInfo(NAME_FULL, ASCIIToUTF16("John Doe"));
  autofill_table_->SetServerProfiles(server_profiles);
  personal_data_->Refresh();
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());

  // Add profile with identical values.  Duplicates should not get saved.
  AutofillProfile scraped_profile(base::GenerateGUID(),
                                  "https://www.example.com");
  test::SetProfileInfo(&scraped_profile, "John", "", "Doe", "", "ACME Corp",
                       "500 Oak View", "Apt 8", "Houston", "TX", "77401", "US",
                       "");
  EXPECT_TRUE(scraped_profile.IsSubsetOf(server_profiles.back(), "en-US"));
  std::string saved_guid = personal_data_->SaveImportedProfile(scraped_profile);
  EXPECT_NE(scraped_profile.guid(), saved_guid);

  personal_data_->Refresh();
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Verify the non-addition.
  EXPECT_EQ(0U, personal_data_->web_profiles().size());
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());

  // Verify that the server profile's use date was updated.
  const AutofillProfile* server_profile = personal_data_->GetProfiles()[0];
  EXPECT_GT(base::TimeDelta::FromMilliseconds(500),
            base::Time::Now() - server_profile->use_date());
}

// Tests that SaveImportedProfile sets the modification date on new profiles.
TEST_F(PersonalDataManagerTest, SaveImportedProfileSetModificationDate) {
  AutofillProfile profile(test::GetFullProfile());
  EXPECT_EQ(base::Time(), profile.modification_date());

  personal_data_->SaveImportedProfile(profile);
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
  EXPECT_GT(base::TimeDelta::FromMilliseconds(500),
            base::Time::Now() - profiles[0]->modification_date());
}

TEST_F(PersonalDataManagerTest, AddUpdateRemoveProfiles) {
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549");

  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile2,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");

  // Add two test profiles to the database.
  personal_data_->AddProfile(profile0);
  personal_data_->AddProfile(profile1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  // Update, remove, and add.
  profile0.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  personal_data_->UpdateProfile(profile0);
  personal_data_->RemoveByGUID(profile1.guid());
  personal_data_->AddProfile(profile2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  profiles.clear();
  profiles.push_back(&profile0);
  profiles.push_back(&profile2);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've loaded the profiles from the web database.
  ExpectSameElements(profiles, personal_data_->GetProfiles());
}

TEST_F(PersonalDataManagerTest, AddUpdateRemoveCreditCards) {
  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0,
      "John Dillinger", "423456789012" /* Visa */, "01", "2010");

  CreditCard credit_card1(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card1,
      "Bonnie Parker", "518765432109" /* Mastercard */, "12", "2012");

  CreditCard credit_card2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card2,
      "Clyde Barrow", "347666888555" /* American Express */, "04", "2015");

  // Add two test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Update, remove, and add.
  credit_card0.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Joe"));
  personal_data_->UpdateCreditCard(credit_card0);
  personal_data_->RemoveByGUID(credit_card1.guid());
  personal_data_->AddCreditCard(credit_card2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've loaded the credit cards from the web database.
  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

TEST_F(PersonalDataManagerTest, UpdateUnverifiedProfilesAndCreditCards) {
  // Start with unverified data.
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  EXPECT_FALSE(profile.IsVerified());

  CreditCard credit_card(base::GenerateGUID(), "https://www.example.com/");
  test::SetCreditCardInfo(&credit_card,
      "John Dillinger", "423456789012" /* Visa */, "01", "2010");
  EXPECT_FALSE(credit_card.IsVerified());

  // Add the data to the database.
  personal_data_->AddProfile(profile);
  personal_data_->AddCreditCard(credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& profiles1 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards1 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles1.size());
  ASSERT_EQ(1U, cards1.size());
  EXPECT_EQ(0, profile.Compare(*profiles1[0]));
  EXPECT_EQ(0, credit_card.Compare(*cards1[0]));

  // Try to update with just the origin changed.
  AutofillProfile original_profile(profile);
  CreditCard original_credit_card(credit_card);
  profile.set_origin("Chrome settings");
  credit_card.set_origin("Chrome settings");

  EXPECT_TRUE(profile.IsVerified());
  EXPECT_TRUE(credit_card.IsVerified());

  personal_data_->UpdateProfile(profile);
  personal_data_->UpdateCreditCard(credit_card);

  // Note: No refresh, as no update is expected.

  const std::vector<AutofillProfile*>& profiles2 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles2.size());
  ASSERT_EQ(1U, cards2.size());
  EXPECT_NE(profile.origin(), profiles2[0]->origin());
  EXPECT_NE(credit_card.origin(), cards2[0]->origin());
  EXPECT_EQ(original_profile.origin(), profiles2[0]->origin());
  EXPECT_EQ(original_credit_card.origin(), cards2[0]->origin());

  // Try to update with data changed as well.
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  credit_card.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Joe"));

  personal_data_->UpdateProfile(profile);
  personal_data_->UpdateCreditCard(credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& profiles3 =
      personal_data_->GetProfiles();
  const std::vector<CreditCard*>& cards3 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, profiles3.size());
  ASSERT_EQ(1U, cards3.size());
  EXPECT_EQ(0, profile.Compare(*profiles3[0]));
  EXPECT_EQ(0, credit_card.Compare(*cards3[0]));
  EXPECT_EQ(profile.origin(), profiles3[0]->origin());
  EXPECT_EQ(credit_card.origin(), cards3[0]->origin());
}

// Tests that server cards are ignored without the flag.
TEST_F(PersonalDataManagerTest, ReturnsServerCreditCards) {
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "9012" /* Visa */, "01", "2010");
  server_cards.back().SetTypeForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b456"));
  test::SetCreditCardInfo(&server_cards.back(), "Bonnie Parker",
                          "2109" /* Mastercard */, "12", "2012");
  server_cards.back().SetTypeForMaskedCard(kMasterCard);

  test::SetServerCreditCards(autofill_table_, server_cards);
  personal_data_->Refresh();

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  EXPECT_EQ(0U, personal_data_->GetCreditCards().size());
}

// Makes sure that full cards are re-masked when full PAN storage is off.
TEST_F(PersonalDataManagerTest, RefuseToStoreFullCard) {
  prefs_->SetBoolean(prefs::kAutofillWalletSyncExperimentEnabled, true);

  // On Linux this should be disabled automatically. Elsewhere, only if the
  // flag is passed.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  EXPECT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableOfferStoreUnmaskedWalletCards));
#else
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableOfferStoreUnmaskedWalletCards);
#endif

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "347666888555" /* American Express */, "04", "2015");
  test::SetServerCreditCards(autofill_table_, server_cards);
  personal_data_->Refresh();

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(CreditCard::MASKED_SERVER_CARD,
            personal_data_->GetCreditCards()[0]->record_type());
}

TEST_F(PersonalDataManagerTest, OfferStoreUnmaskedCards) {
#if defined(OS_CHROMEOS) || defined(OS_WIN) || defined(OS_MACOSX) || \
    defined(OS_IOS) || defined(OS_ANDROID)
  bool should_offer = true;
#elif defined(OS_LINUX)
  bool should_offer = false;
#endif
  EXPECT_EQ(should_offer, OfferStoreUnmaskedCards());
}

// Tests that UpdateServerCreditCard can be used to mask or unmask server cards.
TEST_F(PersonalDataManagerTest, UpdateServerCreditCards) {
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "9012" /* Visa */, "01", "2010");
  server_cards.back().SetTypeForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b456"));
  test::SetCreditCardInfo(&server_cards.back(), "Bonnie Parker",
                          "2109" /* Mastercard */, "12", "2012");
  server_cards.back().SetTypeForMaskedCard(kMasterCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "347666888555" /* American Express */, "04", "2015");

  test::SetServerCreditCards(autofill_table_, server_cards);
  personal_data_->Refresh();

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  ASSERT_EQ(3U, personal_data_->GetCreditCards().size());

  if (!OfferStoreUnmaskedCards()) {
    for (CreditCard* card : personal_data_->GetCreditCards()) {
      EXPECT_EQ(CreditCard::MASKED_SERVER_CARD, card->record_type());
    }
    // The rest of this test doesn't work if we're force-masking all unmasked
    // cards.
    return;
  }

  // The GUIDs will be different, so just compare the data.
  for (size_t i = 0; i < 3; ++i)
    EXPECT_EQ(0, server_cards[i].Compare(*personal_data_->GetCreditCards()[i]));

  CreditCard* unmasked_card = &server_cards.front();
  unmasked_card->set_record_type(CreditCard::FULL_SERVER_CARD);
  unmasked_card->SetNumber(ASCIIToUTF16("423456789012"));
  EXPECT_NE(0, server_cards.front().Compare(
                   *personal_data_->GetCreditCards().front()));
  personal_data_->UpdateServerCreditCard(*unmasked_card);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  for (size_t i = 0; i < 3; ++i)
    EXPECT_EQ(0, server_cards[i].Compare(*personal_data_->GetCreditCards()[i]));

  CreditCard* remasked_card = &server_cards.back();
  remasked_card->set_record_type(CreditCard::MASKED_SERVER_CARD);
  remasked_card->SetNumber(ASCIIToUTF16("8555"));
  EXPECT_NE(
      0, server_cards.back().Compare(*personal_data_->GetCreditCards().back()));
  personal_data_->UpdateServerCreditCard(*remasked_card);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  for (size_t i = 0; i < 3; ++i)
    EXPECT_EQ(0, server_cards[i].Compare(*personal_data_->GetCreditCards()[i]));
}

TEST_F(PersonalDataManagerTest, AddProfilesAndCreditCards) {
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549");

  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0,
      "John Dillinger", "423456789012" /* Visa */, "01", "2010");

  CreditCard credit_card1(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card1,
      "Bonnie Parker", "518765432109" /* Mastercard */, "12", "2012");

  // Add two test profiles to the database.
  personal_data_->AddProfile(profile0);
  personal_data_->AddProfile(profile1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  // Add two test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Determine uniqueness by inserting all of the GUIDs into a set and verifying
  // the size of the set matches the number of GUIDs.
  std::set<std::string> guids;
  guids.insert(profile0.guid());
  guids.insert(profile1.guid());
  guids.insert(credit_card0.guid());
  guids.insert(credit_card1.guid());
  EXPECT_EQ(4U, guids.size());
}

// Test for http://crbug.com/50047. Makes sure that guids are populated
// correctly on load.
TEST_F(PersonalDataManagerTest, PopulateUniqueIDsOnLoad) {
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "y", "", "", "", "", "", "", "", "", "", "", "");

  // Add the profile0 to the db.
  personal_data_->AddProfile(profile0);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2[0]));

  // Add a new profile.
  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1,
      "z", "", "", "", "", "", "", "", "", "", "", "");
  personal_data_->AddProfile(profile1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Make sure the two profiles have different GUIDs, both valid.
  const std::vector<AutofillProfile*>& results3 = personal_data_->GetProfiles();
  ASSERT_EQ(2U, results3.size());
  EXPECT_NE(results3[0]->guid(), results3[1]->guid());
  EXPECT_TRUE(base::IsValidGUID(results3[0]->guid()));
  EXPECT_TRUE(base::IsValidGUID(results3[1]->guid()));
}

TEST_F(PersonalDataManagerTest, SetUniqueCreditCardLabels) {
  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com");
  credit_card0.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("John"));
  CreditCard credit_card1(base::GenerateGUID(), "https://www.example.com");
  credit_card1.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Paul"));
  CreditCard credit_card2(base::GenerateGUID(), "https://www.example.com");
  credit_card2.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Ringo"));
  CreditCard credit_card3(base::GenerateGUID(), "https://www.example.com");
  credit_card3.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Other"));
  CreditCard credit_card4(base::GenerateGUID(), "https://www.example.com");
  credit_card4.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Ozzy"));
  CreditCard credit_card5(base::GenerateGUID(), "https://www.example.com");
  credit_card5.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Dio"));

  // Add the test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);
  personal_data_->AddCreditCard(credit_card4);
  personal_data_->AddCreditCard(credit_card5);

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  cards.push_back(&credit_card2);
  cards.push_back(&credit_card3);
  cards.push_back(&credit_card4);
  cards.push_back(&credit_card5);
  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

TEST_F(PersonalDataManagerTest, SetEmptyProfile) {
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "", "", "", "", "", "", "", "", "", "", "", "");

  // Add the empty profile to the database.
  personal_data_->AddProfile(profile0);

  // Note: no refresh here.

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(0U, results2.size());
}

TEST_F(PersonalDataManagerTest, SetEmptyCreditCard) {
  CreditCard credit_card0(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0, "", "", "", "");

  // Add the empty credit card to the database.
  personal_data_->AddCreditCard(credit_card0);

  // Note: no refresh here.

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that we've loaded the credit cards from the web database.
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(0U, results2.size());
}

TEST_F(PersonalDataManagerTest, Refresh) {
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");

  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549");

  // Add the test profiles to the database.
  personal_data_->AddProfile(profile0);
  personal_data_->AddProfile(profile1);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile2,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549");

  autofill_database_service_->AddAutofillProfile(profile2);

  personal_data_->Refresh();

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  profiles.clear();
  profiles.push_back(&profile0);
  profiles.push_back(&profile1);
  profiles.push_back(&profile2);
  ExpectSameElements(profiles, personal_data_->GetProfiles());

  autofill_database_service_->RemoveAutofillProfile(profile1.guid());
  autofill_database_service_->RemoveAutofillProfile(profile2.guid());

  // Before telling the PDM to refresh, simulate an edit to one of the deleted
  // profiles via a SetProfile update (this would happen if the Autofill window
  // was open with a previous snapshot of the profiles, and something
  // [e.g. sync] removed a profile from the browser.  In this edge case, we will
  // end up in a consistent state by dropping the write).
  profile0.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Mar"));
  profile2.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Jo"));
  personal_data_->UpdateProfile(profile0);
  personal_data_->AddProfile(profile1);
  personal_data_->AddProfile(profile2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(profile0, *results[0]);
}

// ImportAddressProfile tests.

TEST_F(PersonalDataManagerTest, ImportAddressProfile) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL);
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(PersonalDataManagerTest, ImportAddressProfile_BadEmail) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "bogus", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_FALSE(ImportAddressProfile(form_structure));

  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(0U, results.size());
}

// Tests that a 'confirm email' field does not block profile import.
TEST_F(PersonalDataManagerTest, ImportAddressProfile_TwoEmails) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "Name:", "name", "George Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "example@example.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Confirm email:", "confirm_email", "example@example.com", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure));
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
}

// Tests two email fields containing different values blocks profile import.
TEST_F(PersonalDataManagerTest, ImportAddressProfile_TwoDifferentEmails) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "Name:", "name", "George Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "example@example.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email2", "example2@example.com", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_FALSE(ImportAddressProfile(form_structure));
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(0U, results.size());
}

// Tests that not enough filled fields will result in not importing an address.
TEST_F(PersonalDataManagerTest, ImportAddressProfile_NotEnoughFilledFields) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Card number:", "card_number", "4111 1111 1111 1111", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_FALSE(ImportAddressProfile(form_structure));

  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());
  const std::vector<CreditCard*>& cards = personal_data_->GetCreditCards();
  ASSERT_EQ(0U, cards.size());
}

TEST_F(PersonalDataManagerTest, ImportAddressProfile_MinimumAddressUSA) {
  // United States addresses must specifiy one address line, a city, state and
  // zip code.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Barack Obama", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address", "1600 Pennsylvania Avenue", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "DC", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "20500", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "USA", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure));
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
}

TEST_F(PersonalDataManagerTest, ImportAddressProfile_MinimumAddressGB) {
  // British addresses do not require a state/province as the county is usually
  // not requested on forms.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "David Cameron", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address", "10 Downing Street", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "London", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Postcode:", "postcode", "SW1A 2AA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Country:", "country", "United Kingdom", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure));
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
}

TEST_F(PersonalDataManagerTest, ImportAddressProfile_MinimumAddressGI) {
  // Gibraltar has the most minimal set of requirements for a valid address.
  // There are no cities or provinces and no postal/zip code system.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "Name:", "name", "Sir Adrian Johns", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address", "The Convent, Main Street", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "Gibraltar", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure));
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
}

TEST_F(PersonalDataManagerTest,
       ImportAddressProfile_PhoneNumberSplitAcrossMultipleFields) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Phone #:", "home_phone_area_code", "650", "text", &field);
  field.max_length = 3;
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Phone #:", "home_phone_prefix", "555", "text", &field);
  field.max_length = 3;
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Phone #:", "home_phone_suffix", "0000", "text", &field);
  field.max_length = 4;
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", NULL, NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, "(650) 555-0000");
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(PersonalDataManagerTest, ImportAddressProfile_MultilineAddress) {
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:",
      "street_address",
      "21 Laussat St\n"
      "Apt. #42",
      "textarea",
      &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "21 Laussat St", "Apt. #42",
      "San Francisco", "California", "94102", NULL, NULL);
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(PersonalDataManagerTest, ImportAddressProfile_TwoValidProfiles) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure1));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL);
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Now create a completely different profile.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "John", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Adams", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "second@gmail.com", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "22 Laussat St", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure2));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected2, "John", NULL,
      "Adams", "second@gmail.com", NULL, "22 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL);
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&expected);
  profiles.push_back(&expected2);
  ExpectSameElements(profiles, personal_data_->GetProfiles());
}

TEST_F(PersonalDataManagerTest, ImportAddressProfile_SameProfileWithConflict) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address", "1600 Pennsylvania Avenue", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 2:", "address2", "Suite A", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Phone:", "phone", "6505556666", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure1));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "1600 Pennsylvania Avenue",
                       "Suite A", "San Francisco", "California", "94102",
                       nullptr, "(650) 555-6666");
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Now create an updated profile.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address", "1600 Pennsylvania Avenue", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 2:", "address2", "Suite A", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  // Country gets added.
  test::CreateTestFormField("Country:", "country", "USA", "text", &field);
  form2.fields.push_back(field);
  // Same phone number with different formatting doesn't create a new profile.
  test::CreateTestFormField("Phone:", "phone", "650-555-6666", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure2));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  // Phone formatting is updated.  Also, country gets added.
  expected.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("650-555-6666"));
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, ImportAddressProfile_MissingInfoInOld) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure1));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", NULL, NULL, "190 High Street", NULL,
      "Philadelphia", "Pennsylvania", "19106", NULL, NULL);
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Submit a form with new data for the first profile.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure2));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  AutofillProfile expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected2, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "190 High Street", NULL,
      "Philadelphia", "Pennsylvania", "19106", NULL, NULL);
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, ImportAddressProfile_MissingInfoInNew) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Company:", "company", "Government", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure1));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected, "George", NULL,
      "Washington", "theprez@gmail.com", "Government", "190 High Street", NULL,
      "Philadelphia", "Pennsylvania", "19106", NULL, NULL);
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Submit a form with new data for the first profile.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  // Note missing Company field.
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure2));

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  // Expect no change.
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, ImportAddressProfile_InsufficientAddress) {
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Company:", "company", "Government", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  EXPECT_FALSE(ImportAddressProfile(form_structure1));

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());
  const std::vector<CreditCard*>& cards = personal_data_->GetCreditCards();
  ASSERT_EQ(0U, cards.size());
}

// Ensure that if a verified profile already exists, aggregated profiles cannot
// modify it in any way. This also checks the profile merging/matching algorithm
// works: if either the full name OR all the non-empty name pieces match, the
// profile is a match.
TEST_F(PersonalDataManagerTest,
       ImportAddressProfile_ExistingVerifiedProfileWithConflict) {
  // Start with a verified profile.
  AutofillProfile profile(base::GenerateGUID(), "Chrome settings");
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  EXPECT_TRUE(profile.IsVerified());

  // Add the profile to the database.
  personal_data_->AddProfile(profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion Mitchell",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Morrison", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "johnwayne@me.xyz", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "123 Zoo St.", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Hollywood", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "CA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "91601", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure));

  // Wait for the refresh, which in this case is a no-op.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));

  // Try the same thing, but without "Mitchell". The profiles should still match
  // because the non empty name pieces (first and last) match that stored in the
  // profile.
  test::CreateTestFormField("First name:", "first_name", "Marion", "text",
                            &field);
  form.fields[0] = field;

  FormStructure form_structure2(form);
  form_structure2.DetermineHeuristicTypes();
  EXPECT_TRUE(ImportAddressProfile(form_structure2));

  // Wait for the refresh, which in this case is a no-op.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile.Compare(*results2[0]));
}

// ImportCreditCard tests.

// Tests that a valid credit card is extracted.
TEST_F(PersonalDataManagerTest, ImportCreditCard_Valid) {
  // Add a single valid credit card form.
  FormData form;
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2011");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

// Tests that an invalid credit card is not extracted.
TEST_F(PersonalDataManagerTest, ImportCreditCard_Invalid) {
  FormData form;
  AddFullCreditCardForm(&form, "Jim Johansen", "1000000000000000", "02",
                        "2012");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_FALSE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(0U, results.size());
}

// Tests that a valid credit card is extracted when the option text for month
// select can't be parsed but its value can.
TEST_F(PersonalDataManagerTest, ImportCreditCard_MonthSelectInvalidText) {
  // Add a single valid credit card form with an invalid option value.
  FormData form;
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111",
                        "Feb (2)", "2011");
  // Add option values and contents to the expiration month field.
  ASSERT_EQ(ASCIIToUTF16("exp_month"), form.fields[2].name);
  std::vector<base::string16> values;
  values.push_back(ASCIIToUTF16("1"));
  values.push_back(ASCIIToUTF16("2"));
  values.push_back(ASCIIToUTF16("3"));
  std::vector<base::string16> contents;
  contents.push_back(ASCIIToUTF16("Jan (1)"));
  contents.push_back(ASCIIToUTF16("Feb (2)"));
  contents.push_back(ASCIIToUTF16("Mar (3)"));
  form.fields[2].option_values = values;
  form.fields[2].option_contents = contents;

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // See that the invalid option text was converted to the right value.
  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "02",
                          "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(PersonalDataManagerTest, ImportCreditCard_TwoValidCards) {
  // Start with a single valid credit card form.
  FormData form1;
  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2011");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card.
  FormData form2;
  AddFullCreditCardForm(&form2, "", "5500 0000 0000 0004", "02", "2012");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2, false, &imported_credit_card2));
  ASSERT_TRUE(imported_credit_card2);
  personal_data_->SaveImportedCreditCard(*imported_credit_card2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2, "", "5500000000000004", "02", "2012");
  std::vector<CreditCard*> cards;
  cards.push_back(&expected);
  cards.push_back(&expected2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

// Tests that a credit card is extracted because it only matches a masked server
// card.
TEST_F(PersonalDataManagerTest,
       ImportCreditCard_DuplicateServerCards_MaskedCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2010");
  server_cards.back().SetTypeForMaskedCard(kVisaCard);
  test::SetServerCreditCards(autofill_table_, server_cards);

  // Type the same data as the masked card into a form.
  FormData form;
  AddFullCreditCardForm(&form, "John Dillinger", "4111111111111111", "01",
                        "2010");

  // The card should be offered to be saved locally because it only matches the
  // masked server card.
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();
}

// Tests that a credit card is not extracted because it matches a full server
// card.
TEST_F(PersonalDataManagerTest,
       ImportCreditCard_DuplicateServerCards_FullCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "347666888555" /* American Express */, "04", "2015");
  test::SetServerCreditCards(autofill_table_, server_cards);

  // Type the same data as the unmasked card into a form.
  FormData form;
  AddFullCreditCardForm(&form, "Clyde Barrow", "347666888555", "04", "2015");

  // The card should not be offered to be saved locally because it only matches
  // the full server card.
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_FALSE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);
}

TEST_F(PersonalDataManagerTest, ImportCreditCard_SameCreditCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1;
  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2011");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2;
  AddFullCreditCardForm(&form2, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        /* different year */ "2012");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2, false, &imported_credit_card2));
  EXPECT_FALSE(imported_credit_card2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2012".
  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2012");
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, ImportCreditCard_ShouldReturnLocalCard) {
  // Start with a single valid credit card form.
  FormData form1;
  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2011");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2;
  AddFullCreditCardForm(&form2, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        /* different year */ "2012");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2,
                               /* should_return_local_card= */ true,
                               &imported_credit_card2));
  // The local card is returned after an update.
  EXPECT_TRUE(imported_credit_card2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2012".
  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2,
      "Biggie Smalls", "4111111111111111", "01", "2012");
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, ImportCreditCard_EmptyCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1;
  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2011");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second credit card with no number.
  FormData form2;
  AddFullCreditCardForm(&form2, "Biggie Smalls", /* no number */ nullptr, "01",
                        "2012");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card2;
  EXPECT_FALSE(
      ImportCreditCard(form_structure2, false, &imported_credit_card2));
  EXPECT_FALSE(imported_credit_card2);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, ImportCreditCard_MissingInfoInNew) {
  // Start with a single valid credit card form.
  FormData form1;
  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2011");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the name is missing but
  // the credit card number matches.
  FormData form2;
  AddFullCreditCardForm(&form2, /* missing name */ nullptr,
                        "4111-1111-1111-1111", "01", "2011");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2, false, &imported_credit_card2));
  EXPECT_FALSE(imported_credit_card2);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));

  // Add a third credit card where the expiration date is missing.
  FormData form3;
  AddFullCreditCardForm(&form3, "Johnny McEnroe", "5555555555554444",
                        /* no month */ nullptr,
                        /* no year */ nullptr);

  FormStructure form_structure3(form3);
  form_structure3.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card3;
  EXPECT_FALSE(
      ImportCreditCard(form_structure3, false, &imported_credit_card3));
  ASSERT_FALSE(imported_credit_card3);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected3(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected3,
      "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results3 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results3.size());
  EXPECT_EQ(0, expected3.Compare(*results3[0]));
}

TEST_F(PersonalDataManagerTest, ImportCreditCard_MissingInfoInOld) {
  // Start with a single valid credit card stored via the preferences.
  // Note the empty name.
  CreditCard saved_credit_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&saved_credit_card,
      "", "4111111111111111" /* Visa */, "01", "2011");
  personal_data_->AddCreditCard(saved_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<CreditCard*>& results1 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(saved_credit_card, *results1[0]);

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form;
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        /* different year */ "2012");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that the newer information is saved.  In this case the year is
  // added to the existing credit card.
  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2,
      "Biggie Smalls", "4111111111111111", "01", "2012");
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

// We allow the user to store a credit card number with separators via the UI.
// We should not try to re-aggregate the same card with the separators stripped.
TEST_F(PersonalDataManagerTest, ImportCreditCard_SameCardWithSeparators) {
  // Start with a single valid credit card stored via the preferences.
  // Note the separators in the credit card number.
  CreditCard saved_credit_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&saved_credit_card,
      "Biggie Smalls", "4111 1111 1111 1111" /* Visa */, "01", "2011");
  personal_data_->AddCreditCard(saved_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<CreditCard*>& results1 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results1[0]));

  // Import the same card info, but with different separators in the number.
  FormData form;
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2011");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect that no new card is saved.
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results2[0]));
}

// Ensure that if a verified credit card already exists, aggregated credit cards
// cannot modify it in any way.
TEST_F(PersonalDataManagerTest,
       ImportCreditCard_ExistingVerifiedCardWithConflict) {
  // Start with a verified credit card.
  CreditCard credit_card(base::GenerateGUID(), "Chrome settings");
  test::SetCreditCardInfo(&credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2011");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  personal_data_->AddCreditCard(credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Simulate a form submission with conflicting expiration year.
  FormData form;
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        /* different year */ "2012");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect that the saved credit card is not modified.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, credit_card.Compare(*results[0]));
}

// ImportFormData tests (both addresses and credit cards).

// Test that a form with both address and credit card sections imports the
// address and the credit card.
TEST_F(PersonalDataManagerTest, ImportFormData_OneAddressOneCreditCard) {
  FormData form;
  FormFieldData field;
  // Address section.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2011");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure, false,
                                             &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Test that the address has been saved.
  AutofillProfile expected_address(base::GenerateGUID(),
                                   "https://www.example.com");
  test::SetProfileInfo(&expected_address, "George", NULL, "Washington",
                       "theprez@gmail.com", NULL, "21 Laussat St", NULL,
                       "San Francisco", "California", "94102", NULL, NULL);
  const std::vector<AutofillProfile*>& results_addr =
      personal_data_->GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_EQ(0, expected_address.Compare(*results_addr[0]));

  // Test that the credit card has also been saved.
  CreditCard expected_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2011");
  const std::vector<CreditCard*>& results_cards =
      personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results_cards.size());
  EXPECT_EQ(0, expected_card.Compare(*results_cards[0]));
}

// Test that a form with two address sections and a credit card section does not
// import the address but does import the credit card.
TEST_F(PersonalDataManagerTest, ImportFormData_TwoAddressesOneCreditCard) {
  FormData form;
  FormFieldData field;
  // Address section 1.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Address section 2.
  test::CreateTestFormField("Name:", "name", "Barack Obama", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address", "1600 Pennsylvania Avenue",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "DC", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "20500", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "USA", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2011");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  // Still returns true because the credit card import was successful.
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure, false,
                                             &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Test that the address has NOT been saved.
  EXPECT_TRUE(personal_data_->GetProfiles().empty());

  // Test that the credit card has been saved.
  CreditCard expected_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected_card.Compare(*results[0]));
}

// Ensure that verified profiles can be saved via SaveImportedProfile,
// overwriting existing unverified profiles.
TEST_F(PersonalDataManagerTest, SaveImportedProfileWithVerifiedData) {
  // Start with an unverified profile.
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  EXPECT_FALSE(profile.IsVerified());

  // Add the profile to the database.
  personal_data_->AddProfile(profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile new_verified_profile = profile;
  new_verified_profile.set_guid(base::GenerateGUID());
  new_verified_profile.set_origin("Chrome settings");
  new_verified_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                                  ASCIIToUTF16("1 234 567-8910"));
  EXPECT_TRUE(new_verified_profile.IsVerified());

  personal_data_->SaveImportedProfile(new_verified_profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // The new profile should be merged into the existing one.
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, new_verified_profile.Compare(*results[0]));
}

// Ensure that verified profiles can be saved via SaveImportedProfile,
// overwriting existing verified profiles as well.
TEST_F(PersonalDataManagerTest, SaveImportedProfileWithExistingVerifiedData) {
  // Start with a verified profile.
  AutofillProfile profile(base::GenerateGUID(), "Chrome settings");
  test::SetProfileInfo(&profile,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  EXPECT_TRUE(profile.IsVerified());

  // Add the profile to the database.
  personal_data_->AddProfile(profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile new_verified_profile = profile;
  new_verified_profile.set_guid(base::GenerateGUID());
  new_verified_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                                  ASCIIToUTF16("1 234 567-8910"));
  EXPECT_TRUE(new_verified_profile.IsVerified());

  personal_data_->SaveImportedProfile(new_verified_profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // The new profile should be merged into the existing one.
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, new_verified_profile.Compare(*results[0]));
}

// Ensure that verified credit cards can be saved via SaveImportedCreditCard.
TEST_F(PersonalDataManagerTest, SaveImportedCreditCardWithVerifiedData) {
  // Start with a verified credit card.
  CreditCard credit_card(base::GenerateGUID(), "Chrome settings");
  test::SetCreditCardInfo(&credit_card,
      "Biggie Smalls", "4111 1111 1111 1111" /* Visa */, "01", "2011");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  personal_data_->AddCreditCard(credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard new_verified_card = credit_card;
  new_verified_card.set_guid(base::GenerateGUID());
  new_verified_card.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("B. Small"));
  EXPECT_TRUE(new_verified_card.IsVerified());

  personal_data_->SaveImportedCreditCard(new_verified_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that the saved credit card is updated.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(ASCIIToUTF16("B. Small"), results[0]->GetRawInfo(CREDIT_CARD_NAME));
}

TEST_F(PersonalDataManagerTest, GetNonEmptyTypes) {
  // Check that there are no available types with no profiles stored.
  ServerFieldTypeSet non_empty_types;
  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_EQ(0U, non_empty_types.size());

  // Test with one profile stored.
  AutofillProfile profile0(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile0,
      "Marion", NULL, "Morrison",
      "johnwayne@me.xyz", NULL, "123 Zoo St.", NULL, "Hollywood", "CA",
      "91601", "US", "14155678910");

  personal_data_->AddProfile(profile0);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_EQ(15U, non_empty_types.size());
  EXPECT_TRUE(non_empty_types.count(NAME_FIRST));
  EXPECT_TRUE(non_empty_types.count(NAME_LAST));
  EXPECT_TRUE(non_empty_types.count(NAME_FULL));
  EXPECT_TRUE(non_empty_types.count(EMAIL_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE1));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_CITY));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STATE));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_ZIP));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_COUNTRY));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_COUNTRY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_WHOLE_NUMBER));

  // Test with multiple profiles stored.
  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "16502937549");

  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile2,
      "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "16502937549");

  personal_data_->AddProfile(profile1);
  personal_data_->AddProfile(profile2);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_EQ(19U, non_empty_types.size());
  EXPECT_TRUE(non_empty_types.count(NAME_FIRST));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE_INITIAL));
  EXPECT_TRUE(non_empty_types.count(NAME_LAST));
  EXPECT_TRUE(non_empty_types.count(NAME_FULL));
  EXPECT_TRUE(non_empty_types.count(EMAIL_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(COMPANY_NAME));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE1));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE2));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_CITY));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STATE));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_ZIP));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_COUNTRY));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_COUNTRY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_WHOLE_NUMBER));

  // Test with credit card information also stored.
  CreditCard credit_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card,
                          "John Dillinger", "423456789012" /* Visa */,
                          "01", "2010");
  personal_data_->AddCreditCard(credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  personal_data_->GetNonEmptyTypes(&non_empty_types);
  EXPECT_EQ(27U, non_empty_types.size());
  EXPECT_TRUE(non_empty_types.count(NAME_FIRST));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE));
  EXPECT_TRUE(non_empty_types.count(NAME_MIDDLE_INITIAL));
  EXPECT_TRUE(non_empty_types.count(NAME_LAST));
  EXPECT_TRUE(non_empty_types.count(NAME_FULL));
  EXPECT_TRUE(non_empty_types.count(EMAIL_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(COMPANY_NAME));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE1));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_LINE2));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_CITY));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_STATE));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_ZIP));
  EXPECT_TRUE(non_empty_types.count(ADDRESS_HOME_COUNTRY));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_COUNTRY_CODE));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_CITY_AND_NUMBER));
  EXPECT_TRUE(non_empty_types.count(PHONE_HOME_WHOLE_NUMBER));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_NAME));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_NUMBER));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_TYPE));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_MONTH));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_2_DIGIT_YEAR));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR));
  EXPECT_TRUE(non_empty_types.count(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR));
}

TEST_F(PersonalDataManagerTest, IncognitoReadOnly) {
  ASSERT_TRUE(personal_data_->GetProfiles().empty());
  ASSERT_TRUE(personal_data_->GetCreditCards().empty());

  AutofillProfile steve_jobs(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&steve_jobs, "Steven", "Paul", "Jobs", "sjobs@apple.com",
      "Apple Computer, Inc.", "1 Infinite Loop", "", "Cupertino", "CA", "95014",
      "US", "(800) 275-2273");
  personal_data_->AddProfile(steve_jobs);

  CreditCard bill_gates(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(
      &bill_gates, "William H. Gates", "5555555555554444", "1", "2020");
  personal_data_->AddCreditCard(bill_gates);

  // The personal data manager should be able to read existing profiles in an
  // off-the-record context.
  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  ASSERT_EQ(1U, personal_data_->GetProfiles().size());
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // No adds, saves, or updates should take effect.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);

  // Add profiles or credit card shouldn't work.
  personal_data_->AddProfile(test::GetFullProfile());

  CreditCard larry_page(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(
      &larry_page, "Lawrence Page", "4111111111111111", "10", "2025");
  personal_data_->AddCreditCard(larry_page);

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Saving or creating profiles from imported profiles shouldn't work.
  steve_jobs.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Steve"));
  personal_data_->SaveImportedProfile(steve_jobs);

  bill_gates.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Bill Gates"));
  personal_data_->SaveImportedCreditCard(bill_gates);

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(ASCIIToUTF16("Steven"),
            personal_data_->GetProfiles()[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("William H. Gates"),
            personal_data_->GetCreditCards()[0]->GetRawInfo(CREDIT_CARD_NAME));

  // Updating existing profiles shouldn't work.
  steve_jobs.SetRawInfo(NAME_FIRST, ASCIIToUTF16("Steve"));
  personal_data_->UpdateProfile(steve_jobs);

  bill_gates.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Bill Gates"));
  personal_data_->UpdateCreditCard(bill_gates);

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(ASCIIToUTF16("Steven"),
            personal_data_->GetProfiles()[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("William H. Gates"),
            personal_data_->GetCreditCards()[0]->GetRawInfo(CREDIT_CARD_NAME));

  // Removing shouldn't work.
  personal_data_->RemoveByGUID(steve_jobs.guid());
  personal_data_->RemoveByGUID(bill_gates.guid());

  ResetPersonalDataManager(USER_MODE_INCOGNITO);
  EXPECT_EQ(1U, personal_data_->GetProfiles().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeIsCached) {
  // The return value should always be some country code, no matter what.
  std::string default_country =
      personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(2U, default_country.size());

  AutofillProfile moose(base::GenerateGUID(), "Chrome settings");
  test::SetProfileInfo(&moose, "Moose", "P", "McMahon", "mpm@example.com",
      "", "1 Taiga TKTR", "", "Calgary", "AB", "T2B 2K2",
      "CA", "(800) 555-9000");
  personal_data_->AddProfile(moose);
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();
  // The value is cached and doesn't change even after adding an address.
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(2);

  // Disabling Autofill blows away this cache and shouldn't account for Autofill
  // profiles.
  prefs_->SetBoolean(prefs::kAutofillEnabled, false);
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Enabling Autofill blows away the cached value and should reflect the new
  // value (accounting for profiles).
  prefs_->SetBoolean(prefs::kAutofillEnabled, true);
  EXPECT_EQ(base::UTF16ToUTF8(moose.GetRawInfo(ADDRESS_HOME_COUNTRY)),
            personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromProfiles) {
  AutofillProfile moose(base::GenerateGUID(), "Chrome settings");
  test::SetProfileInfo(&moose, "Moose", "P", "McMahon", "mpm@example.com",
      "", "1 Taiga TKTR", "", "Calgary", "AB", "T2B 2K2",
      "CA", "(800) 555-9000");
  personal_data_->AddProfile(moose);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("CA", personal_data_->GetDefaultCountryCodeForNewAddress());

  // Multiple profiles cast votes.
  AutofillProfile armadillo(base::GenerateGUID(), "Chrome settings");
  test::SetProfileInfo(&armadillo, "Armin", "Dill", "Oh", "ado@example.com",
      "", "1 Speed Bump", "", "Lubbock", "TX", "77500",
      "MX", "(800) 555-9000");
  AutofillProfile armadillo2(base::GenerateGUID(), "Chrome settings");
  test::SetProfileInfo(&armadillo2, "Armin", "Dill", "Oh", "ado@example.com",
      "", "2 Speed Bump", "", "Lubbock", "TX", "77500",
      "MX", "(800) 555-9000");
  personal_data_->AddProfile(armadillo);
  personal_data_->AddProfile(armadillo2);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("MX", personal_data_->GetDefaultCountryCodeForNewAddress());

  personal_data_->RemoveByGUID(armadillo.guid());
  personal_data_->RemoveByGUID(armadillo2.guid());
  ResetPersonalDataManager(USER_MODE_NORMAL);
  // Verified profiles count more.
  armadillo.set_origin("http://randomwebsite.com");
  armadillo2.set_origin("http://randomwebsite.com");
  personal_data_->AddProfile(armadillo);
  personal_data_->AddProfile(armadillo2);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("CA", personal_data_->GetDefaultCountryCodeForNewAddress());

  personal_data_->RemoveByGUID(armadillo.guid());
  ResetPersonalDataManager(USER_MODE_NORMAL);
  // But unverified profiles can be a tie breaker.
  armadillo.set_origin("Chrome settings");
  personal_data_->AddProfile(armadillo);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("MX", personal_data_->GetDefaultCountryCodeForNewAddress());

  // Invalid country codes are ignored.
  personal_data_->RemoveByGUID(armadillo.guid());
  personal_data_->RemoveByGUID(moose.guid());
  AutofillProfile space_invader(base::GenerateGUID(), "Chrome settings");
  test::SetProfileInfo(&space_invader, "Marty", "", "Martian",
      "mm@example.com", "", "1 Flying Object", "", "Valles Marineris", "",
      "", "XX", "");
  personal_data_->AddProfile(moose);
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_EQ("MX", personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, UpdateLanguageCodeInProfile) {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  personal_data_->AddProfile(profile);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  profile.set_language_code("en");
  personal_data_->UpdateProfile(profile);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));
  EXPECT_EQ("en", results[0]->language_code());
}

TEST_F(PersonalDataManagerTest, GetProfileSuggestions) {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox",
      "123 Zoo St.\nSecond Line\nThird line", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  personal_data_->AddProfile(profile);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS), base::ASCIIToUTF16("123"),
      false, std::vector<ServerFieldType>());
  ASSERT_FALSE(suggestions.empty());
  EXPECT_EQ(suggestions[0].value,
            base::ASCIIToUTF16("123 Zoo St., Second Line, Third line, unit 5"));
}

TEST_F(PersonalDataManagerTest, GetProfileSuggestions_HideSubsets) {
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");

  // Dupe profile, except different in email address (irrelevant for this form).
  AutofillProfile profile1 = profile;
  profile1.set_guid(base::GenerateGUID());
  profile1.SetRawInfo(EMAIL_ADDRESS, base::ASCIIToUTF16("spam_me@example.com"));

  // Dupe profile, except different in address state.
  AutofillProfile profile2 = profile;
  profile2.set_guid(base::GenerateGUID());
  profile2.SetRawInfo(ADDRESS_HOME_STATE, base::ASCIIToUTF16("TX"));

  // Subset profile.
  AutofillProfile profile3 = profile;
  profile3.set_guid(base::GenerateGUID());
  profile3.SetRawInfo(ADDRESS_HOME_STATE, base::string16());

  // For easier results verification, make sure |profile| is suggested first.
  profile.set_use_count(5);
  personal_data_->AddProfile(profile);
  personal_data_->AddProfile(profile1);
  personal_data_->AddProfile(profile2);
  personal_data_->AddProfile(profile3);
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Simulate a form with street address, city and state.
  std::vector<ServerFieldType> types;
  types.push_back(ADDRESS_HOME_CITY);
  types.push_back(ADDRESS_HOME_STATE);
  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS), base::ASCIIToUTF16("123"),
      false, types);
  ASSERT_EQ(2U, suggestions.size());
  EXPECT_EQ(base::ASCIIToUTF16("Hollywood, CA"), suggestions[0].label);
  EXPECT_EQ(base::ASCIIToUTF16("Hollywood, TX"), suggestions[1].label);
}

// Tests that GetProfileSuggestions orders its suggestions based on the frecency
// formula.
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_Ranking) {
  // Set up the profiles. They are named with number suffixes X so the X is the
  // order in which they should be ordered by frecency.
  AutofillProfile profile3(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile3.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(1));
  profile3.set_use_count(5);
  personal_data_->AddProfile(profile3);

  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile1.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(1));
  profile1.set_use_count(10);
  personal_data_->AddProfile(profile1);

  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  profile2.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(15));
  profile2.set_use_count(300);
  personal_data_->AddProfile(profile2);

  ResetPersonalDataManager(USER_MODE_NORMAL);
  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), base::ASCIIToUTF16("Ma"), false,
      std::vector<ServerFieldType>());
  ASSERT_EQ(3U, suggestions.size());
  EXPECT_EQ(suggestions[0].value, base::ASCIIToUTF16("Marion1"));
  EXPECT_EQ(suggestions[1].value, base::ASCIIToUTF16("Marion2"));
  EXPECT_EQ(suggestions[2].value, base::ASCIIToUTF16("Marion3"));
}

// Tests that GetProfileSuggestions returns all profiles suggestions by default
// and only two if the appropriate field trial is set.
TEST_F(PersonalDataManagerTest, GetProfileSuggestions_NumberOfSuggestions) {
  // Set up 3 different profiles.
  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  personal_data_->AddProfile(profile1);

  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  personal_data_->AddProfile(profile2);

  AutofillProfile profile3(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile3, "Marion3", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  personal_data_->AddProfile(profile3);

  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Verify that all the profiles are suggested.
  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), base::string16(), false,
      std::vector<ServerFieldType>());
  EXPECT_EQ(3U, suggestions.size());

  // Verify that only two profiles are suggested.
  EnableAutofillProfileLimitFieldTrial("2");

  suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), base::string16(), false,
      std::vector<ServerFieldType>());
  EXPECT_EQ(2U, suggestions.size());
}

// Tests that GetProfileSuggestions returns the right number of profile
// suggestions when the limit to three field trial is set and there are less
// than three profiles.
TEST_F(PersonalDataManagerTest,
       GetProfileSuggestions_LimitIsMoreThanProfileSuggestions) {
  EnableAutofillProfileLimitFieldTrial("3");

  // Set up 2 different profiles.
  AutofillProfile profile1(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile1, "Marion1", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  personal_data_->AddProfile(profile1);

  AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&profile2, "Marion2", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox",
                       "123 Zoo St.\nSecond Line\nThird line", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  personal_data_->AddProfile(profile2);

  ResetPersonalDataManager(USER_MODE_NORMAL);

  std::vector<Suggestion> suggestions = personal_data_->GetProfileSuggestions(
      AutofillType(NAME_FIRST), base::string16(), false,
      std::vector<ServerFieldType>());
  EXPECT_EQ(2U, suggestions.size());
}

// Test that local credit cards are ordered as expected.
TEST_F(PersonalDataManagerTest, GetCreditCardSuggestions_LocalCardsRanking) {
  SetupReferenceLocalCreditCards();

  // Sublabel is card number when filling name (exact format depends on
  // the platform, but the last 4 digits should appear).
  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME),
          /* field_contents= */ base::string16());
  ASSERT_EQ(3U, suggestions.size());

  // Ordered as expected.
  EXPECT_EQ(ASCIIToUTF16("John Dillinger"), suggestions[0].value);
  EXPECT_TRUE(suggestions[0].label.find(ASCIIToUTF16("9012")) !=
              base::string16::npos);
  EXPECT_EQ(ASCIIToUTF16("Clyde Barrow"), suggestions[1].value);
  EXPECT_TRUE(suggestions[1].label.find(ASCIIToUTF16("8555")) !=
              base::string16::npos);
  EXPECT_EQ(ASCIIToUTF16("Bonnie Parker"), suggestions[2].value);
  EXPECT_TRUE(suggestions[2].label.find(ASCIIToUTF16("2109")) !=
              base::string16::npos);
}

// Test that local and server cards are ordered as expected.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_LocalAndServerCardsRanking) {
  EnableWalletCardImport();
  SetupReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b459"));
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2012");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(base::Time::Now() -
                                   base::TimeDelta::FromDays(1));
  server_cards.back().SetTypeForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "b460"));
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2012");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(base::Time::Now() -
                                   base::TimeDelta::FromDays(1));

  test::SetServerCreditCards(autofill_table_, server_cards);
  personal_data_->Refresh();
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME),
          /* field_contents= */ base::string16());
  ASSERT_EQ(5U, suggestions.size());

  // All cards should be ordered as expected.
  EXPECT_EQ(ASCIIToUTF16("Jesse James"), suggestions[0].value);
  EXPECT_EQ(ASCIIToUTF16("John Dillinger"), suggestions[1].value);
  EXPECT_EQ(ASCIIToUTF16("Clyde Barrow"), suggestions[2].value);
  EXPECT_EQ(ASCIIToUTF16("Emmet Dalton"), suggestions[3].value);
  EXPECT_EQ(ASCIIToUTF16("Bonnie Parker"), suggestions[4].value);
}

// Test that a card that doesn't have a number is not shown in the suggestions
// when querying credit cards by their number.
TEST_F(PersonalDataManagerTest, GetCreditCardSuggestions_NumberMissing) {
  // Create one normal credit card and one credit card with the number missing.
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "347666888555" /* American Express */, "04", "2015");
  credit_card0.set_use_count(3);
  credit_card0.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(1));
  personal_data_->AddCreditCard(credit_card0);

  CreditCard credit_card1("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          "https://www.example.com");
  credit_card1.set_use_count(300);
  credit_card1.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(10));
  test::SetCreditCardInfo(&credit_card1, "John Dillinger", "", "01", "2010");
  personal_data_->AddCreditCard(credit_card1);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  ASSERT_EQ(2U, personal_data_->GetCreditCards().size());

  // Sublabel is expiration date when filling card number. The second card
  // doesn't have a number so it should not be included in the suggestions.
  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NUMBER),
          /* field_contents= */ base::string16());
  ASSERT_EQ(1U, suggestions.size());
  EXPECT_EQ(UTF8ToUTF16("Amex\xC2\xA0\xE2\x8B\xAF"
                        "8555"),
            suggestions[0].value);
  EXPECT_EQ(ASCIIToUTF16("04/15"), suggestions[0].label);
}

// Tests the suggestions of duplicate local and server credit cards.
TEST_F(PersonalDataManagerTest, GetCreditCardSuggestions_ServerDuplicates) {
  EnableWalletCardImport();
  SetupReferenceLocalCreditCards();

  // Add some server cards. If there are local dupes, the locals should be
  // hidden.
  std::vector<CreditCard> server_cards;
  // This server card matches a local card, except the local card is missing the
  // number. This should count as a dupe and thus not be shown in the
  // suggestions since the locally saved card takes precedence.
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "9012" /* Visa */, "01", "2010");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(base::Time::Now() -
                                   base::TimeDelta::FromDays(15));
  server_cards.back().SetTypeForMaskedCard(kVisaCard);

  // This server card is identical to a local card, but has a different
  // card type. Not a dupe and therefore both should appear in the suggestions.
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b456"));
  test::SetCreditCardInfo(&server_cards.back(), "Bonnie Parker", "2109", "12",
                          "2012");
  server_cards.back().set_use_count(3);
  server_cards.back().set_use_date(base::Time::Now() -
                                   base::TimeDelta::FromDays(15));
  server_cards.back().SetTypeForMaskedCard(kVisaCard);

  // This unmasked server card is an exact dupe of a local card. Therefore only
  // this card should appear in the suggestions as full server cards have
  // precedence over local cards.
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "347666888555" /* American Express */, "04", "2015");
  server_cards.back().set_use_count(1);
  server_cards.back().set_use_date(base::Time::Now() -
                                   base::TimeDelta::FromDays(15));

  test::SetServerCreditCards(autofill_table_, server_cards);
  personal_data_->Refresh();
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME),
          /* field_contents= */ base::string16());
  ASSERT_EQ(4U, suggestions.size());
  EXPECT_EQ(ASCIIToUTF16("John Dillinger"), suggestions[0].value);
  EXPECT_EQ(ASCIIToUTF16("Clyde Barrow"), suggestions[1].value);
  EXPECT_EQ(ASCIIToUTF16("Bonnie Parker"), suggestions[2].value);
  EXPECT_EQ(ASCIIToUTF16("Bonnie Parker"), suggestions[3].value);

  suggestions = personal_data_->GetCreditCardSuggestions(
      AutofillType(CREDIT_CARD_NUMBER), /* field_contents= */ base::string16());
  ASSERT_EQ(4U, suggestions.size());
  EXPECT_EQ(UTF8ToUTF16("Visa\xC2\xA0\xE2\x8B\xAF"
                        "9012"),
            suggestions[0].value);
  EXPECT_EQ(UTF8ToUTF16("Amex\xC2\xA0\xE2\x8B\xAF"
                        "8555"),
            suggestions[1].value);
  EXPECT_EQ(UTF8ToUTF16("MasterCard\xC2\xA0\xE2\x8B\xAF"
                        "2109"),
            suggestions[2].value);
  EXPECT_EQ(UTF8ToUTF16("Visa\xC2\xA0\xE2\x8B\xAF"
                        "2109"),
            suggestions[3].value);
}

// Tests that a full server card can be a dupe of more than one local card.
TEST_F(PersonalDataManagerTest,
       GetCreditCardSuggestions_ServerCardDuplicateOfMultipleLocalCards) {
  EnableWalletCardImport();
  SetupReferenceLocalCreditCards();

  // Add a duplicate server card.
  std::vector<CreditCard> server_cards;
  // This unmasked server card is an exact dupe of a local card. Therefore only
  // the local card should appear in the suggestions.
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "347666888555" /* American Express */, "04", "2015");

  test::SetServerCreditCards(autofill_table_, server_cards);
  personal_data_->Refresh();
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  std::vector<Suggestion> suggestions =
      personal_data_->GetCreditCardSuggestions(
          AutofillType(CREDIT_CARD_NAME),
          /* field_contents= */ base::string16());
  ASSERT_EQ(3U, suggestions.size());

  // Add a second dupe local card to make sure a full server card can be a dupe
  // of more than one local card.
  CreditCard credit_card3("4141084B-72D7-4B73-90CF-3D6AC154673B",
                          "https://www.example.com");
  test::SetCreditCardInfo(&credit_card3, "Clyde Barrow", "", "04", "");
  personal_data_->AddCreditCard(credit_card3);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  suggestions = personal_data_->GetCreditCardSuggestions(
      AutofillType(CREDIT_CARD_NAME), /* field_contents= */ base::string16());
  ASSERT_EQ(3U, suggestions.size());
}

// Tests that only the full server card is kept when deduping with a local
// duplicate of it.
TEST_F(PersonalDataManagerTest,
       DedupeCreditCardSuggestions_FullServerShadowsLocal) {
  std::list<const CreditCard*> credit_cards;

  // Create 3 different local credit cards.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        "https://www.example.com");
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "423456789012" /* Visa */, "01", "2010");
  local_card.set_use_count(3);
  local_card.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(1));
  credit_cards.push_back(&local_card);

  // Create a full server card that is a duplicate of one of the local cards.
  CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "423456789012" /* Visa */, "01", "2010");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(base::Time::Now() -
                                base::TimeDelta::FromDays(15));
  credit_cards.push_back(&full_server_card);

  PersonalDataManager::DedupeCreditCardSuggestions(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  const CreditCard* deduped_card(credit_cards.front());
  EXPECT_TRUE(*deduped_card == full_server_card);
}

// Tests that only the local card is kept when deduping with a masked server
// duplicate of it.
TEST_F(PersonalDataManagerTest,
       DedupeCreditCardSuggestions_LocalShadowsMasked) {
  std::list<const CreditCard*> credit_cards;

  CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                        "https://www.example.com");
  local_card.set_use_count(300);
  local_card.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(10));
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "423456789012" /* Visa */, "01", "2010");
  credit_cards.push_back(&local_card);

  // Create a masked server card that is a duplicate of a local card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "9012" /* Visa */,
                          "01", "2010");
  masked_card.set_use_count(2);
  masked_card.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(15));
  masked_card.SetTypeForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  PersonalDataManager::DedupeCreditCardSuggestions(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  const CreditCard* deduped_card(credit_cards.front());
  EXPECT_TRUE(*deduped_card == local_card);
}

// Tests that identical full server and masked credit cards are not deduped.
TEST_F(PersonalDataManagerTest,
       DedupeCreditCardSuggestions_FullServerAndMasked) {
  std::list<const CreditCard*> credit_cards;

  // Create a full server card that is a duplicate of one of the local cards.
  CreditCard full_server_card(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "423456789012" /* Visa */, "01", "2010");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(base::Time::Now() -
                                base::TimeDelta::FromDays(15));
  credit_cards.push_back(&full_server_card);

  // Create a masked server card that is a duplicate of a local card.
  CreditCard masked_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "9012" /* Visa */,
                          "01", "2010");
  masked_card.set_use_count(2);
  masked_card.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(15));
  masked_card.SetTypeForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  PersonalDataManager::DedupeCreditCardSuggestions(&credit_cards);
  EXPECT_EQ(2U, credit_cards.size());
}

// Tests that slightly different local, full server, and masked credit cards are
// not deduped.
TEST_F(PersonalDataManagerTest, DedupeCreditCardSuggestions_DifferentCards) {
  std::list<const CreditCard*> credit_cards;

  CreditCard credit_card2("002149C1-EE28-4213-A3B9-DA243FFF021B",
                          "https://www.example.com");
  credit_card2.set_use_count(1);
  credit_card2.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(1));
  test::SetCreditCardInfo(&credit_card2, "Homer Simpson",
                          "518765432109" /* Mastercard */, "", "");
  credit_cards.push_back(&credit_card2);

  // Create a masked server card that is slightly different of the local card.
  CreditCard credit_card4(CreditCard::MASKED_SERVER_CARD, "b456");
  test::SetCreditCardInfo(&credit_card4, "Homer Simpson", "2109", "12", "2012");
  credit_card4.set_use_count(3);
  credit_card4.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(15));
  credit_card4.SetTypeForMaskedCard(kVisaCard);
  credit_cards.push_back(&credit_card4);

  // Create a full server card that is slightly different of the two other
  // cards.
  CreditCard credit_card5(CreditCard::FULL_SERVER_CARD, "c789");
  test::SetCreditCardInfo(&credit_card5, "Homer Simpson",
                          "347666888555" /* American Express */, "04", "2015");
  credit_card5.set_use_count(1);
  credit_card5.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(15));
  credit_cards.push_back(&credit_card5);

  PersonalDataManager::DedupeCreditCardSuggestions(&credit_cards);
  EXPECT_EQ(3U, credit_cards.size());
}

TEST_F(PersonalDataManagerTest, RecordUseOf) {
  AutofillProfile profile(test::GetFullProfile());
  EXPECT_EQ(0U, profile.use_count());
  EXPECT_EQ(base::Time(), profile.use_date());
  EXPECT_EQ(base::Time(), profile.modification_date());
  personal_data_->AddProfile(profile);

  CreditCard credit_card(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "423456789012" /* Visa */, "01", "2010");
  EXPECT_EQ(0U, credit_card.use_count());
  EXPECT_EQ(base::Time(), credit_card.use_date());
  EXPECT_EQ(base::Time(), credit_card.modification_date());
  personal_data_->AddCreditCard(credit_card);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Notify the PDM that the profile and credit card were used.
  AutofillProfile* added_profile =
      personal_data_->GetProfileByGUID(profile.guid());
  ASSERT_TRUE(added_profile);
  EXPECT_EQ(*added_profile, profile);
  EXPECT_EQ(0U, added_profile->use_count());
  EXPECT_EQ(base::Time(), added_profile->use_date());
  EXPECT_NE(base::Time(), added_profile->modification_date());
  personal_data_->RecordUseOf(profile);

  CreditCard* added_card =
      personal_data_->GetCreditCardByGUID(credit_card.guid());
  ASSERT_TRUE(added_card);
  EXPECT_EQ(*added_card, credit_card);
  EXPECT_EQ(0U, added_card->use_count());
  EXPECT_EQ(base::Time(), added_card->use_date());
  EXPECT_NE(base::Time(), added_card->modification_date());
  personal_data_->RecordUseOf(credit_card);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Verify usage stats are updated.
  added_profile = personal_data_->GetProfileByGUID(profile.guid());
  ASSERT_TRUE(added_profile);
  EXPECT_EQ(1U, added_profile->use_count());
  EXPECT_NE(base::Time(), added_profile->use_date());
  EXPECT_NE(base::Time(), added_profile->modification_date());

  added_card = personal_data_->GetCreditCardByGUID(credit_card.guid());
  ASSERT_TRUE(added_card);
  EXPECT_EQ(1U, added_card->use_count());
  EXPECT_NE(base::Time(), added_card->use_date());
  EXPECT_NE(base::Time(), added_card->modification_date());
}

TEST_F(PersonalDataManagerTest, UpdateServerCreditCardUsageStats) {
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "9012" /* Visa */, "01", "2010");
  server_cards.back().SetTypeForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "b456"));
  test::SetCreditCardInfo(&server_cards.back(), "Bonnie Parker",
                          "4444" /* Mastercard */, "12", "2012");
  server_cards.back().SetTypeForMaskedCard(kMasterCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "347666888555" /* American Express */, "04", "2015");

  test::SetServerCreditCards(autofill_table_, server_cards);
  personal_data_->Refresh();

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  ASSERT_EQ(3U, personal_data_->GetCreditCards().size());

  if (!OfferStoreUnmaskedCards()) {
    for (CreditCard* card : personal_data_->GetCreditCards()) {
      EXPECT_EQ(CreditCard::MASKED_SERVER_CARD, card->record_type());
    }
    // The rest of this test doesn't work if we're force-masking all unmasked
    // cards.
    return;
  }

  // The GUIDs will be different, so just compare the data.
  for (size_t i = 0; i < 3; ++i)
    EXPECT_EQ(0, server_cards[i].Compare(*personal_data_->GetCreditCards()[i]));

  CreditCard* unmasked_card = &server_cards.front();
  unmasked_card->set_record_type(CreditCard::FULL_SERVER_CARD);
  unmasked_card->SetNumber(ASCIIToUTF16("423456789012"));
  EXPECT_NE(0, unmasked_card->Compare(
      *personal_data_->GetCreditCards().front()));
  personal_data_->UpdateServerCreditCard(*unmasked_card);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();
  ASSERT_EQ(3U, personal_data_->GetCreditCards().size());

  for (size_t i = 0; i < 3; ++i)
    EXPECT_EQ(0, server_cards[i].Compare(*personal_data_->GetCreditCards()[i]));

  // For an unmasked card, usage data starts out as 1 and Now().
  EXPECT_EQ(1U, personal_data_->GetCreditCards()[0]->use_count());
  EXPECT_NE(base::Time(), personal_data_->GetCreditCards()[0]->use_date());

  EXPECT_EQ(0U, personal_data_->GetCreditCards()[1]->use_count());
  EXPECT_EQ(base::Time(), personal_data_->GetCreditCards()[1]->use_date());

  // Having unmasked this card, usage stats should be 1 and Now().
  EXPECT_EQ(1U, personal_data_->GetCreditCards()[2]->use_count());
  EXPECT_NE(base::Time(), personal_data_->GetCreditCards()[2]->use_date());
  base::Time initial_use_date = personal_data_->GetCreditCards()[2]->use_date();

  server_cards.back().set_guid(personal_data_->GetCreditCards()[2]->guid());
  personal_data_->RecordUseOf(server_cards.back());
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();
  ASSERT_EQ(3U, personal_data_->GetCreditCards().size());

  EXPECT_EQ(1U, personal_data_->GetCreditCards()[0]->use_count());
  EXPECT_NE(base::Time(), personal_data_->GetCreditCards()[0]->use_date());

  EXPECT_EQ(0U, personal_data_->GetCreditCards()[1]->use_count());
  EXPECT_EQ(base::Time(), personal_data_->GetCreditCards()[1]->use_date());

  EXPECT_EQ(2U, personal_data_->GetCreditCards()[2]->use_count());
  EXPECT_NE(base::Time(), personal_data_->GetCreditCards()[2]->use_date());
  // Time may or may not have elapsed between unmasking and RecordUseOf.
  EXPECT_LE(initial_use_date, personal_data_->GetCreditCards()[2]->use_date());

  // Can record usage stats on masked cards.
  server_cards[1].set_guid(personal_data_->GetCreditCards()[1]->guid());
  personal_data_->RecordUseOf(server_cards[1]);
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();
  ASSERT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards()[1]->use_count());
  EXPECT_NE(base::Time(), personal_data_->GetCreditCards()[1]->use_date());

  // Upgrading to unmasked retains the usage stats (and increments them).
  CreditCard* unmasked_card2 = &server_cards[1];
  unmasked_card2->set_record_type(CreditCard::FULL_SERVER_CARD);
  unmasked_card2->SetNumber(ASCIIToUTF16("5555555555554444"));
  personal_data_->UpdateServerCreditCard(*unmasked_card2);

  server_cards[1].set_guid(personal_data_->GetCreditCards()[1]->guid());
  personal_data_->RecordUseOf(server_cards[1]);
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();
  ASSERT_EQ(3U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCards()[1]->use_count());
  EXPECT_NE(base::Time(), personal_data_->GetCreditCards()[1]->use_date());
}

TEST_F(PersonalDataManagerTest, ClearAllServerData) {
  // Add a server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "9012" /* Visa */, "01", "2010");
  server_cards.back().SetTypeForMaskedCard(kVisaCard);
  test::SetServerCreditCards(autofill_table_, server_cards);
  personal_data_->Refresh();

  // Need to set the google services username
  EnableWalletCardImport();

  // Add a server profile.
  std::vector<AutofillProfile> server_profiles;
  server_profiles.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, "a123"));
  test::SetProfileInfo(&server_profiles.back(), "John", "", "Doe", "",
                       "ACME Corp", "500 Oak View", "Apt 8", "Houston", "TX",
                       "77401", "US", "");
  autofill_table_->SetServerProfiles(server_profiles);

  // The card and profile should be there.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_FALSE(personal_data_->GetCreditCards().empty());
  EXPECT_FALSE(personal_data_->GetProfiles().empty());

  personal_data_->ClearAllServerData();

  // Reload the database, everything should be gone.
  ResetPersonalDataManager(USER_MODE_NORMAL);
  EXPECT_TRUE(personal_data_->GetCreditCards().empty());
  EXPECT_TRUE(personal_data_->GetProfiles().empty());
}

TEST_F(PersonalDataManagerTest, DontDuplicateServerCard) {
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1881" /* Visa */, "01", "2017");
  server_cards.back().SetTypeForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "347666888555" /* American Express */, "04", "2015");

  test::SetServerCreditCards(autofill_table_, server_cards);
  personal_data_->Refresh();
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // A valid credit card form. A user re-types one of their masked cards.
  // We should offer to save.
  FormData form1;
  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John Dillinger",
                            "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4012888888881881",
                            "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2017", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1, false,
                                             &imported_credit_card));
  EXPECT_TRUE(imported_credit_card);

  // A user re-types (or fills with) an unmasked card. Don't offer to save
  // again.
  FormData form2;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "347666888555",
                            "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2015", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  scoped_ptr<CreditCard> imported_credit_card2;
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure2, false,
                                              &imported_credit_card2));
  EXPECT_FALSE(imported_credit_card2);
}

// Tests the SaveImportedProfile method with different profiles to make sure the
// merge logic works correctly.
TEST_F(PersonalDataManagerTest, SaveImportedProfile) {
  typedef struct {
    autofill::ServerFieldType field_type;
    std::string field_value;
  } ProfileField;

  typedef std::vector<ProfileField> ProfileFields;

  typedef struct {
    // Each test starts with a default pre-existing profile and applies these
    // changes to it.
    ProfileFields changes_to_original;
    // Each test saves a second profile. Applies these changes to the default
    // values before saving.
    ProfileFields changes_to_new;
    // For tests with profile merging, makes sure that these fields' values are
    // the ones we expect (depending on the test).
    ProfileFields changed_field_values;
  } TestCase;

  TestCase test_cases[] = {
      // Test that saving an identical profile except for the name results in
      // two profiles being saved.
      {ProfileFields(), {{NAME_FIRST, "Marionette"}}},

      // Test that saving an identical profile except for the first address line
      // results in two profiles being saved.
      {ProfileFields(), {{ADDRESS_HOME_LINE1, "123 Aquarium St."}}},

      // Test that saving an identical profile except for the second address
      // line results in two profiles being saved.
      {ProfileFields(), {{ADDRESS_HOME_LINE2, "unit 7"}}},

      // Tests that saving an identical profile that has a new piece of
      // information (company name) results in a merge and that the original
      // empty value gets overwritten by the new information.
      {{{COMPANY_NAME, ""}}, ProfileFields(), {{COMPANY_NAME, "Fox"}}},

      // Tests that saving an identical profile except a loss of information
      // results in a merge but the original value is not overwritten (no
      // information loss).
      {ProfileFields(), {{COMPANY_NAME, ""}}, {{COMPANY_NAME, "Fox"}}},

      // Tests that saving an identical profile except a slightly different
      // postal code results in a merge with the new value kept.
      {{{ADDRESS_HOME_ZIP, "R2C 0A1"}}, {{ADDRESS_HOME_ZIP, "R2C0A1"}},
        {{ADDRESS_HOME_ZIP, "R2C0A1"}}},
      {{{ADDRESS_HOME_ZIP, "R2C0A1"}}, {{ADDRESS_HOME_ZIP, "R2C 0A1"}},
        {{ADDRESS_HOME_ZIP, "R2C 0A1"}}},
      {{{ADDRESS_HOME_ZIP, "r2c 0a1"}}, {{ADDRESS_HOME_ZIP, "R2C0A1"}},
        {{ADDRESS_HOME_ZIP, "R2C0A1"}}},

      // Tests that saving an identical profile plus a new piece of information
      // on the address line 2 results in a merge and that the original empty
      // value gets overwritten by the new information.
      {{{ADDRESS_HOME_LINE2, ""}},
       ProfileFields(),
       {{ADDRESS_HOME_LINE2, "unit 5"}}},

      // Tests that saving an identical profile except a loss of information on
      // the address line 2 results in a merge but that the original value gets
      // not overwritten (no information loss).
      {ProfileFields(),
       {{ADDRESS_HOME_LINE2, ""}},
       {{ADDRESS_HOME_LINE2, "unit 5"}}},

      // Tests that saving an identical except with more punctuation in the fist
      // address line, while the second is empty, results in a merge and that
      // the original address gets overwritten.
      {{{ADDRESS_HOME_LINE2, ""}},
       {{ADDRESS_HOME_LINE2, ""}, {ADDRESS_HOME_LINE1, "123, Zoo St."}},
       {{ADDRESS_HOME_LINE1, "123, Zoo St."}}},

      // Tests that saving an identical profile except with less punctuation in
      // the fist address line, while the second is empty, results in a merge
      // and that the original address gets overwritten.
      {{{ADDRESS_HOME_LINE2, ""}, {ADDRESS_HOME_LINE1, "123, Zoo St."}},
       {{ADDRESS_HOME_LINE2, ""}},
       {{ADDRESS_HOME_LINE1, "123 Zoo St"}}},

      // Tests that saving an identical profile except additional punctuation in
      // the two address lines results in a merge and that the original address
      // gets overwritten.
      {ProfileFields(),
       {{ADDRESS_HOME_LINE1, "123, Zoo St."}, {ADDRESS_HOME_LINE2, "unit. 5"}},
       {{ADDRESS_HOME_LINE1, "123, Zoo St."}, {ADDRESS_HOME_LINE2, "unit. 5"}}},

      // Tests that saving an identical profile except less punctuation in the
      // two address lines results in a merge and that the original address gets
      // overwritten.
      {{{ADDRESS_HOME_LINE1, "123, Zoo St."}, {ADDRESS_HOME_LINE2, "unit. 5"}},
       ProfileFields(),
       {{ADDRESS_HOME_LINE1, "123 Zoo St"}, {ADDRESS_HOME_LINE2, "unit 5"}}},

      // Tests that saving an identical profile except that the address line 1
      // is in the address line 2 results in a merge and that the original
      // address lines do not get overwritten.
      {ProfileFields(),
       {{ADDRESS_HOME_LINE1, "123 Zoo St, unit 5"}, {ADDRESS_HOME_LINE2, ""}},
       {{ADDRESS_HOME_LINE1, "123 Zoo St"}, {ADDRESS_HOME_LINE2, "unit 5"}}},

      // Tests that saving an identical profile except that the address line 2
      // contains part of the old address line 1 results in a merge and that the
      // original address lines of the reference profile get overwritten.
      {{{ADDRESS_HOME_LINE1, "123 Zoo St, unit 5"}, {ADDRESS_HOME_LINE2, ""}},
       ProfileFields(),
       {{ADDRESS_HOME_LINE1, "123 Zoo St"}, {ADDRESS_HOME_LINE2, "unit 5"}}},

      // Tests that saving an identical profile except that the state is the
      // abbreviation instead of the full form results in a merge and that the
      // original state gets overwritten.
      {{{ADDRESS_HOME_STATE, "California"}},
       ProfileFields(),
       {{ADDRESS_HOME_STATE, "CA"}}},

      // Tests that saving an identical profile except that the state is the
      // full form instead of the abbreviation results in a  merge and that the
      // original state gets overwritten.
      {ProfileFields(),
       {{ADDRESS_HOME_STATE, "California"}},
       {{ADDRESS_HOME_STATE, "California"}}},
  };

  for (TestCase test_case : test_cases) {
    SetupReferenceProfile();

    const std::vector<AutofillProfile*>& initial_profiles =
        personal_data_->GetProfiles();

    // Apply changes to the original profile (if applicable).
    for (ProfileField change : test_case.changes_to_original) {
      initial_profiles.front()->SetRawInfo(
          change.field_type, base::UTF8ToUTF16(change.field_value));
    }

    AutofillProfile profile2(base::GenerateGUID(), "https://www.example.com");
    test::SetProfileInfo(&profile2, "Marion", "Mitchell", "Morrison",
                         "johnwayne@me.xyz", "Fox", "123 Zoo St", "unit 5",
                         "Hollywood", "CA", "91601", "US", "12345678910");

    // Apply changes to the second profile (if applicable).
    for (ProfileField change : test_case.changes_to_new) {
      profile2.SetRawInfo(change.field_type,
                          base::UTF8ToUTF16(change.field_value));
    }

    personal_data_->SaveImportedProfile(profile2);

    const std::vector<AutofillProfile*>& saved_profiles =
        personal_data_->GetProfiles();

    // If there are no merge changes to verify, make sure that two profiles were
    // saved.
    if (test_case.changed_field_values.empty()) {
      EXPECT_EQ(2U, saved_profiles.size());
    } else {
      // Make sure the new information was merged correctly.
      for (ProfileField changed_field : test_case.changed_field_values) {
        EXPECT_EQ(base::UTF8ToUTF16(changed_field.field_value),
                  saved_profiles.front()->GetRawInfo(changed_field.field_type));
      }
      // Verify that the merged profile's modification and use dates were
      // updated.
      EXPECT_GT(
          base::TimeDelta::FromMilliseconds(500),
          base::Time::Now() - saved_profiles.front()->modification_date());
      EXPECT_GT(base::TimeDelta::FromMilliseconds(500),
                base::Time::Now() - saved_profiles.front()->use_date());
    }

    // Erase the profiles for the next test.
    ResetProfiles();
  }
}

}  // namespace autofill
