// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_common_unittest.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "webkit/glue/form_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using webkit_glue::FormData;

ACTION(QuitUIMessageLoop) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  MessageLoop::current()->Quit();
}

class PersonalDataLoadedObserverMock : public PersonalDataManager::Observer {
 public:
  PersonalDataLoadedObserverMock() {}
  virtual ~PersonalDataLoadedObserverMock() {}

  MOCK_METHOD0(OnPersonalDataLoaded, void());
};

class PersonalDataManagerTest : public testing::Test {
 protected:
  PersonalDataManagerTest()
      : ui_thread_(ChromeThread::UI, &message_loop_),
        db_thread_(ChromeThread::DB) {
  }

  virtual void SetUp() {
#if defined(OS_MACOSX)
    Encryptor::UseMockKeychain(true);
#endif

    db_thread_.Start();

    profile_.reset(new TestingProfile);
    profile_->CreateWebDataService(false);

    ResetPersonalDataManager();
  }

  virtual void TearDown() {
    if (profile_.get())
      profile_.reset(NULL);

    db_thread_.Stop();
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask);
    MessageLoop::current()->Run();
  }

  void ResetPersonalDataManager() {
    personal_data_ = new PersonalDataManager();
    personal_data_->Init(profile_.get());
    personal_data_->SetObserver(&personal_data_observer_);

    // Disable auxiliary profiles for unit testing.  These reach out to system
    // services on the Mac.
    profile_->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  }

  AutoFillProfile* MakeProfile() {
    AutoLock lock(personal_data_->unique_ids_lock_);
    return new AutoFillProfile(string16(),
        personal_data_->CreateNextUniqueID(
        &personal_data_->unique_profile_ids_));
  }

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  ChromeThread db_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<PersonalDataManager> personal_data_;
  NotificationRegistrar registrar_;
  NotificationObserverMock observer_;
  PersonalDataLoadedObserverMock personal_data_observer_;
};

// TODO(jhawkins): Test SetProfiles w/out a WebDataService in the profile.
TEST_F(PersonalDataManagerTest, SetProfiles) {
  AutoFillProfile profile0(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile profile1(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile1,
      "Home", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549", "13502849239");

  AutoFillProfile profile2(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile2,
      "Work", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add two test profiles to the database.
  std::vector<AutoFillProfile> update;
  update.push_back(profile0);
  update.push_back(profile1);
  personal_data_->SetProfiles(&update);

  // The PersonalDataManager will update the unique IDs when saving the
  // profiles, so we have to update the expectations.
  profile0.set_unique_id(update[0].unique_id());
  profile1.set_unique_id(update[1].unique_id());

  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(profile0, *results1.at(0));
  EXPECT_EQ(profile1, *results1.at(1));

  // Three operations in one:
  //  - Update profile0
  //  - Remove profile1
  //  - Add profile2
  profile0.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("John"));
  update.clear();
  update.push_back(profile0);
  update.push_back(profile2);
  personal_data_->SetProfiles(&update);

  // Set the expected unique ID for profile2.
  profile2.set_unique_id(update[1].unique_id());

  // AutoFillProfile IDs are re-used, so the third profile to be added will have
  // a unique ID that matches the old unique ID of the removed profile1, even
  // though that ID has already been used.
  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(profile0, *results2.at(0));
  EXPECT_EQ(profile2, *results2.at(1));
  EXPECT_EQ(profile1.unique_id(), profile2.unique_id());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager();

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the PersonalDataLoadedObserver is notified.
  MessageLoop::current()->Run();

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutoFillProfile*>& results3 = personal_data_->profiles();
  ASSERT_EQ(2U, results3.size());
  EXPECT_EQ(profile0, *results3.at(0));
  EXPECT_EQ(profile2, *results3.at(1));
}

// TODO(jhawkins): Test SetCreditCards w/out a WebDataService in the profile.
TEST_F(PersonalDataManagerTest, SetCreditCards) {
  CreditCard creditcard0(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard0, "Corporate",
      "John Dillinger", "Visa", "123456789012", "01", "2010", "Chicago");

  CreditCard creditcard1(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard1, "Personal",
      "Bonnie Parker", "Mastercard", "098765432109", "12", "2012", "Dallas");

  CreditCard creditcard2(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard2, "Savings", "Clyde Barrow",
      "American Express", "777666888555", "04", "2015", "Home");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add two test credit cards to the database.
  std::vector<CreditCard> update;
  update.push_back(creditcard0);
  update.push_back(creditcard1);
  personal_data_->SetCreditCards(&update);

  // The PersonalDataManager will update the unique IDs when saving the
  // credit cards, so we have to update the expectations.
  creditcard0.set_unique_id(update[0].unique_id());
  creditcard1.set_unique_id(update[1].unique_id());

  const std::vector<CreditCard*>& results1 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(creditcard0, *results1.at(0));
  EXPECT_EQ(creditcard1, *results1.at(1));

  // Three operations in one:
  //  - Update creditcard0
  //  - Remove creditcard1
  //  - Add creditcard2
  creditcard0.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  update.clear();
  update.push_back(creditcard0);
  update.push_back(creditcard2);
  personal_data_->SetCreditCards(&update);

  // Set the expected unique ID for creditcard2.
  creditcard2.set_unique_id(update[1].unique_id());

  // CreditCard IDs are re-used, so the third credit card to be added will have
  // a unique ID that matches the old unique ID of the removed creditcard1, even
  // though that ID has already been used.
  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(creditcard0, *results2.at(0));
  EXPECT_EQ(creditcard2, *results2.at(1));
  EXPECT_EQ(creditcard2.unique_id(), creditcard1.unique_id());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the PersonalDataLoadedObserver is notified.
  MessageLoop::current()->Run();

  // Verify that we've loaded the credit cards from the web database.
  const std::vector<CreditCard*>& results3 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results3.size());
  EXPECT_EQ(creditcard0, *results3.at(0));
  EXPECT_EQ(creditcard2, *results3.at(1));
}

TEST_F(PersonalDataManagerTest, SetProfilesAndCreditCards) {
  AutoFillProfile profile0(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile profile1(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile1,
      "Home", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549", "13502849239");

  CreditCard creditcard0(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard0, "Corporate",
      "John Dillinger", "Visa", "123456789012", "01", "2010", "Chicago");

  CreditCard creditcard1(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard1, "Personal",
      "Bonnie Parker", "Mastercard", "098765432109", "12", "2012", "Dallas");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(
      personal_data_observer_,
      OnPersonalDataLoaded()).Times(2).WillRepeatedly(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add two test profiles to the database.
  std::vector<AutoFillProfile> update;
  update.push_back(profile0);
  update.push_back(profile1);
  personal_data_->SetProfiles(&update);

  // The PersonalDataManager will update the unique IDs when saving the
  // profiles, so we have to update the expectations.
  profile0.set_unique_id(update[0].unique_id());
  profile1.set_unique_id(update[1].unique_id());

  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(profile0, *results1.at(0));
  EXPECT_EQ(profile1, *results1.at(1));

  // Add two test credit cards to the database.
  std::vector<CreditCard> update_cc;
  update_cc.push_back(creditcard0);
  update_cc.push_back(creditcard1);
  personal_data_->SetCreditCards(&update_cc);

  // The PersonalDataManager will update the unique IDs when saving the
  // credit cards, so we have to update the expectations.
  creditcard0.set_unique_id(update_cc[0].unique_id());
  creditcard1.set_unique_id(update_cc[1].unique_id());

  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(creditcard0, *results2.at(0));
  EXPECT_EQ(creditcard1, *results2.at(1));

  // Determine uniqueness by inserting all of the IDs into a set and verifying
  // the size of the set matches the number of IDs.
  std::set<int> ids;
  ids.insert(profile0.unique_id());
  ids.insert(profile1.unique_id());
  ids.insert(creditcard0.unique_id());
  ids.insert(creditcard1.unique_id());
  EXPECT_EQ(4U, ids.size());
}

TEST_F(PersonalDataManagerTest, SetEmptyProfile) {
  AutoFillProfile profile0(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile0,
      "", "", "", "", "", "", "", "", "", "", "", "", "", "");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add the empty profile to the database.
  std::vector<AutoFillProfile> update;
  update.push_back(profile0);
  personal_data_->SetProfiles(&update);

  // Check the local store of profiles, not yet saved to the web database.
  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(0U, results1.size());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the profiles from the web
  // database.
  ResetPersonalDataManager();

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the PersonalDataLoadedObserver is notified.
  MessageLoop::current()->Run();

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();
  ASSERT_EQ(0U, results2.size());
}

TEST_F(PersonalDataManagerTest, SetEmptyCreditCard) {
  CreditCard creditcard0(string16(), 0);
  autofill_unittest::SetCreditCardInfo(&creditcard0,
      "", "", "", "", "", "", "");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add the empty credit card to the database.
  std::vector<CreditCard> update;
  update.push_back(creditcard0);
  personal_data_->SetCreditCards(&update);

  // Check the local store of credit cards, not yet saved to the web database.
  const std::vector<CreditCard*>& results1 = personal_data_->credit_cards();
  ASSERT_EQ(0U, results1.size());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the PersonalDataLoadedObserver is notified.
  MessageLoop::current()->Run();

  // Verify that we've loaded the credit cards from the web database.
  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(0U, results2.size());
}

TEST_F(PersonalDataManagerTest, Refresh) {
  AutoFillProfile profile0(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile profile1(string16(), 0);
  autofill_unittest::SetProfileInfo(&profile1,
      "Home", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549", "13502849239");

  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  // Add the test profiles to the database.
  std::vector<AutoFillProfile> update;
  update.push_back(profile0);
  update.push_back(profile1);
  personal_data_->SetProfiles(&update);

  profile0.set_unique_id(update[0].unique_id());
  profile1.set_unique_id(update[1].unique_id());

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(profile0, *results1.at(0));
  EXPECT_EQ(profile1, *results1.at(1));

  scoped_ptr<AutoFillProfile> profile2(MakeProfile());
  autofill_unittest::SetProfileInfo(profile2.get(),
      "Work", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  WebDataService* wds = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  ASSERT_TRUE(wds);
  wds->AddAutoFillProfile(*profile2.get());

  personal_data_->Refresh();

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
    OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();
  ASSERT_EQ(3U, results2.size());
  EXPECT_EQ(profile0, *results2.at(0));
  EXPECT_EQ(profile1, *results2.at(1));
  EXPECT_EQ(*profile2.get(), *results2.at(2));

  wds->RemoveAutoFillProfile(profile1.unique_id());
  wds->RemoveAutoFillProfile(profile2->unique_id());

  // Before telling the PDM to refresh, simulate an edit to one of the profiles
  // via a SetProfile update (this would happen if the AutoFill window was
  // open with a previous snapshot of the profiles, and something [e.g. sync]
  // removed a profile from the browser.  In this edge case, we will end up
  // in a consistent state by dropping the write).
  profile2->SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Jo"));
  update.clear();
  update.push_back(profile0);
  update.push_back(profile1);
  update.push_back(*profile2.get());
  personal_data_->SetProfiles(&update);

  // And wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results3 = personal_data_->profiles();
  ASSERT_EQ(1U, results3.size());
  EXPECT_EQ(profile0, *results2.at(0));
}

TEST_F(PersonalDataManagerTest, ImportFormData) {
  FormData form;
  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  std::vector<FormStructure*> forms;
  forms.push_back(&form_structure);
  personal_data_->ImportFormData(forms, NULL);

  // And wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  AutoFillProfile expected(ASCIIToUTF16("Unlabeled"), 1);
  autofill_unittest::SetProfileInfo(&expected, "Unlabeled", "George", NULL,
      "Washington", "theprez@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL);
  const std::vector<AutoFillProfile*>& results = personal_data_->profiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(expected, *results[0]);
}

TEST_F(PersonalDataManagerTest, SetUniqueProfileLabels) {
  AutoFillProfile profile0(ASCIIToUTF16("Home"), 0);
  profile0.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("John"));
  AutoFillProfile profile1(ASCIIToUTF16("Home"), 0);
  profile1.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Paul"));
  AutoFillProfile profile2(ASCIIToUTF16("Home"), 0);
  profile2.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Ringo"));
  AutoFillProfile profile3(ASCIIToUTF16("NotHome"), 0);
  profile3.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Other"));
  AutoFillProfile profile4(ASCIIToUTF16("Work"), 0);
  profile4.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Ozzy"));
  AutoFillProfile profile5(ASCIIToUTF16("Work"), 0);
  profile5.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Dio"));

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add the test profiles to the database.
  std::vector<AutoFillProfile> update;
  update.push_back(profile0);
  update.push_back(profile1);
  update.push_back(profile2);
  update.push_back(profile3);
  update.push_back(profile4);
  update.push_back(profile5);
  personal_data_->SetProfiles(&update);

  // And wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results = personal_data_->profiles();
  ASSERT_EQ(6U, results.size());
  EXPECT_EQ(ASCIIToUTF16("Home"), results[0]->Label());
  EXPECT_EQ(ASCIIToUTF16("Home2"), results[1]->Label());
  EXPECT_EQ(ASCIIToUTF16("Home3"), results[2]->Label());
  EXPECT_EQ(ASCIIToUTF16("NotHome"), results[3]->Label());
  EXPECT_EQ(ASCIIToUTF16("Work"), results[4]->Label());
  EXPECT_EQ(ASCIIToUTF16("Work2"), results[5]->Label());
}

TEST_F(PersonalDataManagerTest, SetUniqueCreditCardLabels) {
  CreditCard credit_card0(ASCIIToUTF16("Home"), 0);
  credit_card0.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("John"));
  CreditCard credit_card1(ASCIIToUTF16("Home"), 0);
  credit_card1.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Paul"));
  CreditCard credit_card2(ASCIIToUTF16("Home"), 0);
  credit_card2.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Ringo"));
  CreditCard credit_card3(ASCIIToUTF16("NotHome"), 0);
  credit_card3.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Other"));
  CreditCard credit_card4(ASCIIToUTF16("Work"), 0);
  credit_card4.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Ozzy"));
  CreditCard credit_card5(ASCIIToUTF16("Work"), 0);
  credit_card5.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Dio"));

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add the test credit cards to the database.
  std::vector<CreditCard> update;
  update.push_back(credit_card0);
  update.push_back(credit_card1);
  update.push_back(credit_card2);
  update.push_back(credit_card3);
  update.push_back(credit_card4);
  update.push_back(credit_card5);
  personal_data_->SetCreditCards(&update);

  const std::vector<CreditCard*>& results = personal_data_->credit_cards();
  ASSERT_EQ(6U, results.size());
  EXPECT_EQ(ASCIIToUTF16("Home"), results[0]->Label());
  EXPECT_EQ(ASCIIToUTF16("Home2"), results[1]->Label());
  EXPECT_EQ(ASCIIToUTF16("Home3"), results[2]->Label());
  EXPECT_EQ(ASCIIToUTF16("NotHome"), results[3]->Label());
  EXPECT_EQ(ASCIIToUTF16("Work"), results[4]->Label());
  EXPECT_EQ(ASCIIToUTF16("Work2"), results[5]->Label());
}

TEST_F(PersonalDataManagerTest, AggregateProfileData) {
  scoped_ptr<FormData> form(new FormData);

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form->fields.push_back(field);

  scoped_ptr<FormStructure> form_structure(new FormStructure(*form));
  scoped_ptr<std::vector<FormStructure*> > forms(
      new std::vector<FormStructure*>);
  forms->push_back(form_structure.get());
  personal_data_->ImportFormData(*forms, NULL);

  // And wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  scoped_ptr<AutoFillProfile> expected(
      new AutoFillProfile(ASCIIToUTF16("Unlabeled"), 1));
  autofill_unittest::SetProfileInfo(expected.get(), "Unlabeled", "George", NULL,
      "Washington", "theprez@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL);
  const std::vector<AutoFillProfile*>& results = personal_data_->profiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(*expected, *results[0]);

  // Now create a completely different profile.
  form.reset(new FormData);
  autofill_unittest::CreateTestFormField(
      "First name:", "first_name", "John", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Last name:", "last_name", "Adams", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Email:", "email", "second@gmail.com", "text", &field);
  form->fields.push_back(field);

  form_structure.reset(new FormStructure(*form));
  forms.reset(new std::vector<FormStructure*>);
  forms->push_back(form_structure.get());
  personal_data_->ImportFormData(*forms, NULL);

  // And wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();
  ASSERT_EQ(2U, results2.size());

  expected.reset(new AutoFillProfile(ASCIIToUTF16("Unlabeled"), 1));
  autofill_unittest::SetProfileInfo(expected.get(), "Unlabeled", "George", NULL,
      "Washington", "theprez@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL);
  EXPECT_EQ(*expected, *results2[0]);

  expected.reset(new AutoFillProfile(ASCIIToUTF16("Unlabeled2"), 2));
  autofill_unittest::SetProfileInfo(expected.get(), "Unlabeled2", "John", NULL,
      "Adams", "second@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL);
  EXPECT_EQ(*expected, *results2[1]);

  // Submit a form with new data for the first profile.
  form.reset(new FormData);
  autofill_unittest::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "City:", "city", "Philadelphia", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "State:", "state", "Pennsylvania", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Zip:", "zipcode", "19106", "text", &field);
  form->fields.push_back(field);

  form_structure.reset(new FormStructure(*form));
  forms.reset(new std::vector<FormStructure*>);
  forms->push_back(form_structure.get());
  personal_data_->ImportFormData(*forms, NULL);

  // And wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results3 = personal_data_->profiles();
  ASSERT_EQ(2U, results3.size());

  expected.reset(new AutoFillProfile(ASCIIToUTF16("Unlabeled"), 1));
  autofill_unittest::SetProfileInfo(expected.get(), "Unlabeled", "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "190 High Street", NULL,
      "Philadelphia", "Pennsylvania", "19106", NULL, NULL, NULL);
  EXPECT_EQ(*expected, *results3[0]);

  expected.reset(new AutoFillProfile(ASCIIToUTF16("Unlabeled2"), 2));
  autofill_unittest::SetProfileInfo(expected.get(), "Unlabeled2", "John", NULL,
      "Adams", "second@gmail.com", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL);
  EXPECT_EQ(*expected, *results3[1]);
}
