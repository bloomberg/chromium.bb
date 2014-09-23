// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace {

enum UserMode { USER_MODE_NORMAL, USER_MODE_INCOGNITO };

bool ReturnTrue(const AutofillProfile&) { return true; }

ACTION(QuitMainMessageLoop) { base::MessageLoop::current()->Quit(); }

class PersonalDataLoadedObserverMock : public PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() {}
  virtual ~PersonalDataLoadedObserverMock() {}

  MOCK_METHOD0(OnPersonalDataChanged, void());
};

// Unlike the base AutofillMetrics, exposes copy and assignment constructors,
// which are handy for briefer test code.  The AutofillMetrics class is
// stateless, so this is safe.
class TestAutofillMetrics : public AutofillMetrics {
 public:
  TestAutofillMetrics() {}
  virtual ~TestAutofillMetrics() {}
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
  PersonalDataManagerTest() {}

  virtual void SetUp() {
    prefs_ = test::PrefServiceForTesting();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath path = temp_dir_.path().AppendASCII("TestWebDB");
    web_database_ = new WebDatabaseService(path,
                                           base::MessageLoopProxy::current(),
                                           base::MessageLoopProxy::current());
    web_database_->AddTable(
        scoped_ptr<WebDatabaseTable>(new AutofillTable("en-US")));
    web_database_->LoadDatabase();
    autofill_database_service_ =
        new AutofillWebDataService(web_database_,
                                   base::MessageLoopProxy::current(),
                                   base::MessageLoopProxy::current(),
                                   WebDataServiceBase::ProfileErrorCallback());
    autofill_database_service_->Init();

    test::DisableSystemServices(prefs_.get());
    ResetPersonalDataManager(USER_MODE_NORMAL);
  }

  void ResetPersonalDataManager(UserMode user_mode) {
    bool is_incognito = (user_mode == USER_MODE_INCOGNITO);
    personal_data_.reset(new PersonalDataManager("en-US"));
    personal_data_->Init(
        scoped_refptr<AutofillWebDataService>(autofill_database_service_),
        prefs_.get(),
        is_incognito);
    personal_data_->AddObserver(&personal_data_observer_);

    // Verify that the web database has been updated and the notification sent.
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillOnce(QuitMainMessageLoop());
    base::MessageLoop::current()->Run();
  }

  // The temporary directory should be deleted at the end to ensure that
  // files are not used anymore and deletion succeeds.
  base::ScopedTempDir temp_dir_;
  base::MessageLoopForUI message_loop_;
  scoped_ptr<PrefService> prefs_;
  scoped_refptr<AutofillWebDataService> autofill_database_service_;
  scoped_refptr<WebDatabaseService> web_database_;
  PersonalDataLoadedObserverMock personal_data_observer_;
  scoped_ptr<PersonalDataManager> personal_data_;
};

TEST_F(PersonalDataManagerTest, AddProfile) {
  // Add profile0 to the database.
  AutofillProfile profile0(autofill::test::GetFullProfile());
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

TEST_F(PersonalDataManagerTest, ImportFormData) {
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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

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

TEST_F(PersonalDataManagerTest, ImportFormDataBadEmail) {
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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure,
                                              &imported_credit_card));
  ASSERT_EQ(static_cast<CreditCard*>(NULL), imported_credit_card.get());

  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(0U, results.size());
}

// Tests that a 'confirm email' field does not block profile import.
TEST_F(PersonalDataManagerTest, ImportFormDataTwoEmails) {
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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
}

// Tests two email fields containing different values blocks provile import.
TEST_F(PersonalDataManagerTest, ImportFormDataTwoDifferentEmails) {
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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure,
                                              &imported_credit_card));
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(0U, results.size());
}

TEST_F(PersonalDataManagerTest, ImportFormDataNotEnoughFilledFields) {
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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure,
                                              &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());
  const std::vector<CreditCard*>& cards = personal_data_->GetCreditCards();
  ASSERT_EQ(0U, cards.size());
}

TEST_F(PersonalDataManagerTest, ImportFormMinimumAddressUSA) {
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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                              &imported_credit_card));
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
}

TEST_F(PersonalDataManagerTest, ImportFormMinimumAddressGB) {
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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
}

TEST_F(PersonalDataManagerTest, ImportFormMinimumAddressGI) {
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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(1U, profiles.size());
}

TEST_F(PersonalDataManagerTest, ImportPhoneNumberSplitAcrossMultipleFields) {
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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

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

TEST_F(PersonalDataManagerTest, ImportFormDataMultilineAddress) {
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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

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

TEST_F(PersonalDataManagerTest, AggregateTwoDifferentProfiles) {
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
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

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
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

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

TEST_F(PersonalDataManagerTest, AggregateTwoProfilesWithMultiValue) {
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
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

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
  test::CreateTestFormField("Last name:", "last_name", "Adams", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "second@gmail.com", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  // Modify expected to include multi-valued fields.
  std::vector<base::string16> first_names, last_names, emails;
  expected.GetRawMultiInfo(NAME_FIRST, &first_names);
  first_names.push_back(ASCIIToUTF16("John"));
  expected.GetRawMultiInfo(NAME_LAST, &last_names);
  last_names.push_back(ASCIIToUTF16("Adams"));
  expected.SetRawMultiInfo(NAME_FIRST, first_names);
  expected.SetRawMultiInfo(NAME_LAST, last_names);

  expected.GetRawMultiInfo(EMAIL_ADDRESS, &emails);
  emails.push_back(ASCIIToUTF16("second@gmail.com"));
  expected.SetRawMultiInfo(EMAIL_ADDRESS, emails);

  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateSameProfileWithConflict) {
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
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(
      &expected, "George", NULL, "Washington", "theprez@gmail.com", NULL,
      "1600 Pennsylvania Avenue", "Suite A", "San Francisco", "California",
      "94102", NULL, "(650) 555-6666");
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
  // Phone gets updated.
  test::CreateTestFormField("Phone:", "phone", "6502231234", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  // Add multi-valued phone number to expectation.  Also, country gets added.
  std::vector<base::string16> values;
  expected.GetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, &values);
  values.push_back(ASCIIToUTF16("(650) 223-1234"));
  expected.SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, values);
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateProfileWithMissingInfoInOld) {
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
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

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
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

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

TEST_F(PersonalDataManagerTest, AggregateProfileWithMissingInfoInNew) {
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
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

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
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  // Expect no change.
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateProfileWithInsufficientAddress) {
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
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure1,
                                              &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  ASSERT_EQ(0U, profiles.size());
  const std::vector<CreditCard*>& cards = personal_data_->GetCreditCards();
  ASSERT_EQ(0U, cards.size());
}

TEST_F(PersonalDataManagerTest, AggregateExistingAuxiliaryProfile) {
  // Simulate having access to an auxiliary profile.
  // |auxiliary_profile| will be owned by |personal_data_|.
  AutofillProfile* auxiliary_profile =
      new AutofillProfile(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(auxiliary_profile,
      "Tester", "Frederick", "McAddressBookTesterson",
      "tester@example.com", "Acme Inc.", "1 Main", "Apt A", "San Francisco",
      "CA", "94102", "US", "1.415.888.9999");
  ScopedVector<AutofillProfile>& auxiliary_profiles =
      personal_data_->auxiliary_profiles_;
  auxiliary_profiles.push_back(auxiliary_profile);

  // Simulate a form submission with a subset of the info.
  // Note that the phone number format is different from the saved format.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "Tester", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "McAddressBookTesterson", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "tester@example.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "1 Main", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "CA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone:", "phone", "4158889999", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Note: No refresh.

  // Expect no change.
  const std::vector<AutofillProfile*>& web_profiles =
      personal_data_->web_profiles();
  EXPECT_EQ(0U, web_profiles.size());
  ASSERT_EQ(1U, auxiliary_profiles.size());
  EXPECT_EQ(0, auxiliary_profile->Compare(*auxiliary_profiles[0]));
}

TEST_F(PersonalDataManagerTest, AggregateTwoDifferentCreditCards) {
  FormData form1;

  // Start with a single valid credit card form.
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
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

  // Add a second different valid credit card.
  FormData form2;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "5500 0000 0000 0004", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "02", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  CreditCard expected2(base::GenerateGUID(), "https://www.example.com");
  test::SetCreditCardInfo(&expected2,"", "5500000000000004", "02", "2012");
  std::vector<CreditCard*> cards;
  cards.push_back(&expected);
  cards.push_back(&expected2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

TEST_F(PersonalDataManagerTest, AggregateInvalidCreditCard) {
  FormData form1;

  // Start with a single valid credit card form.
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
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

  // Add a second different invalid credit card.
  FormData form2;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Jim Johansen", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "1000000000000000", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "02", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure2,
                                              &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateSameCreditCardWithConflict) {
  FormData form1;

  // Start with a single valid credit card form.
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
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
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111 1111 1111 1111", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

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

TEST_F(PersonalDataManagerTest, AggregateEmptyCreditCardWithConflict) {
  FormData form1;

  // Start with a single valid credit card form.
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
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
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure2,
                                              &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

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

TEST_F(PersonalDataManagerTest, AggregateCreditCardWithMissingInfoInNew) {
  FormData form1;

  // Start with a single valid credit card form.
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
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
  // Note missing name.
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111111111111111", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

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
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Johnny McEnroe", "text", &field);
  form3.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "5555555555554444", "text", &field);
  form3.fields.push_back(field);
  // Note missing expiration month and year..

  FormStructure form_structure3(form3);
  form_structure3.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(personal_data_->ImportFormData(form_structure3,
                                              &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

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

TEST_F(PersonalDataManagerTest, AggregateCreditCardWithMissingInfoInOld) {
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
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
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
TEST_F(PersonalDataManagerTest, AggregateSameCreditCardWithSeparators) {
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
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2011", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect that no new card is saved.
  const std::vector<CreditCard*>& results2 = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results2[0]));
}

// Ensure that if a verified profile already exists, aggregated profiles cannot
// modify it in any way.
TEST_F(PersonalDataManagerTest, AggregateExistingVerifiedProfileWithConflict) {
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

  // Simulate a form submission with conflicting info.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "First name:", "first_name", "Marion", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Morrison", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "other.email@example.com", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "123 Zoo St.", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Hollywood", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "CA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "91601", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Wait for the refresh, which in this case is a no-op.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));
}

// Ensure that if a verified credit card already exists, aggregated credit cards
// cannot modify it in any way.
TEST_F(PersonalDataManagerTest,
       AggregateExistingVerifiedCreditCardWithConflict) {
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

  // Simulate a form submission with conflicting expiration year.
  FormData form;
  FormFieldData field;
  test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField(
      "Card Number:", "card_number", "4111 1111 1111 1111", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2012", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect that the saved credit card is not modified.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, credit_card.Compare(*results[0]));
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
  new_verified_profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Fizzbang, Inc."));
  EXPECT_TRUE(new_verified_profile.IsVerified());

  personal_data_->SaveImportedProfile(new_verified_profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Expect that the existing profile is not modified, and instead the new
  // profile is added.
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
  new_verified_profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Fizzbang, Inc."));
  new_verified_profile.SetRawInfo(NAME_MIDDLE, base::string16());
  EXPECT_TRUE(new_verified_profile.IsVerified());

  personal_data_->SaveImportedProfile(new_verified_profile);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // The new profile should be merged into the existing one.
  AutofillProfile expected_profile = new_verified_profile;
  expected_profile.set_guid(profile.guid());
  std::vector<base::string16> first_names, middle_names, last_names;
  expected_profile.GetRawMultiInfo(NAME_FIRST, &first_names);
  expected_profile.GetRawMultiInfo(NAME_MIDDLE, &middle_names);
  expected_profile.GetRawMultiInfo(NAME_LAST, &last_names);
  first_names.insert(first_names.begin(), ASCIIToUTF16("Marion"));
  middle_names.insert(middle_names.begin(), ASCIIToUTF16("Mitchell"));
  last_names.insert(last_names.begin(), ASCIIToUTF16("Morrison"));
  expected_profile.SetRawMultiInfo(NAME_FIRST, first_names);
  expected_profile.SetRawMultiInfo(NAME_MIDDLE, middle_names);
  expected_profile.SetRawMultiInfo(NAME_LAST, last_names);

  const std::vector<AutofillProfile*>& results = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(expected_profile, *results[0]);
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

TEST_F(PersonalDataManagerTest, CaseInsensitiveMultiValueAggregation) {
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
  test::CreateTestFormField(
      "City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField(
      "Phone number:", "phone_number", "817-555-6789", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes(TestAutofillMetrics());
  scoped_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure1,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  AutofillProfile expected(base::GenerateGUID(), "https://www.example.com");
  test::SetProfileInfo(&expected,
                       "George",
                       NULL,
                       "Washington",
                       "theprez@gmail.com",
                       NULL,
                       "21 Laussat St",
                       NULL,
                       "San Francisco",
                       "California",
                       "94102",
                       NULL,
                       "817-555-6789");
  const std::vector<AutofillProfile*>& results1 = personal_data_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Upper-case the first name and change the phone number.
  FormData form2;
  test::CreateTestFormField(
      "First name:", "first_name", "GEORGE", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField(
      "Phone number:", "phone_number", "214-555-1234", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(personal_data_->ImportFormData(form_structure2,
                                             &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Verify that the web database has been updated and the notification sent.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  const std::vector<AutofillProfile*>& results2 = personal_data_->GetProfiles();

  // Modify expected to include multi-valued fields.
  std::vector<base::string16> values;
  expected.GetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, &values);
  values.push_back(ASCIIToUTF16("214-555-1234"));
  expected.SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, values);

  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
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

  std::vector<base::string16> values;
  std::vector<base::string16> labels;
  std::vector<base::string16> icons;
  std::vector<PersonalDataManager::GUIDPair> guid_pairs;
  personal_data_->GetProfileSuggestions(
      AutofillType(ADDRESS_HOME_STREET_ADDRESS),
      base::UTF8ToUTF16("123"),
      false,
      std::vector<ServerFieldType>(),
      base::Bind(ReturnTrue),
      &values,
      &labels,
      &icons,
      &guid_pairs);
  ASSERT_FALSE(values.empty());
  EXPECT_EQ(values[0],
      base::UTF8ToUTF16("123 Zoo St., Second Line, Third line, unit 5"));
}

TEST_F(PersonalDataManagerTest, GetCreditCardSuggestions) {
  // These GUIDs are alphabetical to make validating expectations easier.
  CreditCard credit_card0("087151C8-6AB1-487C-9095-28E80BE5DA15",
                          "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0,
      "Clyde Barrow", "347666888555" /* American Express */, "04", "2015");
  personal_data_->AddCreditCard(credit_card0);

  CreditCard credit_card1("6141084B-72D7-4B73-90CF-3D6AC154673B",
                          "https://www.example.com");
  test::SetCreditCardInfo(&credit_card1, "John Dillinger", "", "01", "2010");
  personal_data_->AddCreditCard(credit_card1);

  CreditCard credit_card2("702149C1-EE28-4213-A3B9-DA243FFF021B",
                          "https://www.example.com");
  test::SetCreditCardInfo(&credit_card2,
      "Bonnie Parker", "518765432109" /* Mastercard */, "", "");
  personal_data_->AddCreditCard(credit_card2);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  // Sublabel is card number when filling name.
  std::vector<base::string16> values;
  std::vector<base::string16> labels;
  std::vector<base::string16> icons;
  std::vector<PersonalDataManager::GUIDPair> guid_pairs;
  personal_data_->GetCreditCardSuggestions(
      AutofillType(CREDIT_CARD_NAME),
      base::string16(),
      &values,
      &labels,
      &icons,
      &guid_pairs);
  ASSERT_EQ(3U, values.size());
  ASSERT_EQ(values.size(), labels.size());
  EXPECT_EQ(ASCIIToUTF16("Clyde Barrow"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("*8555"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("John Dillinger"), values[1]);
  EXPECT_EQ(base::string16(), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("Bonnie Parker"), values[2]);
  EXPECT_EQ(ASCIIToUTF16("*2109"), labels[2]);

  // Sublabel is expiration date when filling card number.
  values.clear();
  labels.clear();
  icons.clear();
  guid_pairs.clear();
  personal_data_->GetCreditCardSuggestions(
      AutofillType(CREDIT_CARD_NUMBER),
      base::string16(),
      &values,
      &labels,
      &icons,
      &guid_pairs);
  ASSERT_EQ(2U, values.size());
  ASSERT_EQ(values.size(), labels.size());
  EXPECT_EQ(ASCIIToUTF16("********8555"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("04/15"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("********2109"), values[1]);
  EXPECT_EQ(base::string16(), labels[1]);
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
TEST_F(PersonalDataManagerTest, ShowAddressBookPrompt) {
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(2);

  AutofillType type(ADDRESS_HOME_STREET_ADDRESS);

  prefs_->SetBoolean(prefs::kAutofillEnabled, false);
  EXPECT_FALSE(personal_data_->ShouldShowAccessAddressBookSuggestion(type));

  prefs_->SetBoolean(prefs::kAutofillEnabled, true);
  EXPECT_TRUE(personal_data_->ShouldShowAccessAddressBookSuggestion(type));

  // Adding an Autofill Profile should prevent the prompt from appearing.
  AutofillProfile profile(base::GenerateGUID(), "https://www.example.com/");
  test::SetProfileInfo(&profile,
      "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910");
  personal_data_->AddProfile(profile);

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .WillOnce(QuitMainMessageLoop());
  base::MessageLoop::current()->Run();

  EXPECT_FALSE(personal_data_->ShouldShowAccessAddressBookSuggestion(type));
}

// Tests that the logic to show the access Address Book prompt respects the
// preference that indicates the total number of times the prompt has already
// been shown.
TEST_F(PersonalDataManagerTest, MaxTimesToShowAddressBookPrompt) {
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);

  AutofillType type(ADDRESS_HOME_STREET_ADDRESS);

  prefs_->SetBoolean(prefs::kAutofillEnabled, true);
  EXPECT_TRUE(personal_data_->ShouldShowAccessAddressBookSuggestion(type));

  prefs_->SetInteger(prefs::kAutofillMacAddressBookShowedCount, 4);
  EXPECT_TRUE(personal_data_->ShouldShowAccessAddressBookSuggestion(type));

  prefs_->SetInteger(prefs::kAutofillMacAddressBookShowedCount, 6);
  EXPECT_FALSE(personal_data_->ShouldShowAccessAddressBookSuggestion(type));
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

}  // namespace autofill
