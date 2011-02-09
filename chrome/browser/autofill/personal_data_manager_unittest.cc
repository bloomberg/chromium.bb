// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/common/guid.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/testing_profile.h"
#include "webkit/glue/form_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using webkit_glue::FormData;

ACTION(QuitUIMessageLoop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {
  }

  virtual void SetUp() {
    db_thread_.Start();

    profile_.reset(new TestingProfile);
    profile_->CreateWebDataService(false);

    autofill_test::DisableSystemServices(profile_.get());
    ResetPersonalDataManager();
  }

  virtual void TearDown() {
    personal_data_ = NULL;
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
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<PersonalDataManager> personal_data_;
  NotificationRegistrar registrar_;
  NotificationObserverMock observer_;
  PersonalDataLoadedObserverMock personal_data_observer_;
};

// TODO(jhawkins): Test SetProfiles w/out a WebDataService in the profile.
TEST_F(PersonalDataManagerTest, SetProfiles) {
  AutoFillProfile profile0;
  autofill_test::SetProfileInfo(&profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile profile1;
  autofill_test::SetProfileInfo(&profile1,
      "Home", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549", "13502849239");

  AutoFillProfile profile2;
  autofill_test::SetProfileInfo(&profile2,
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

  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(0, profile0.Compare(*results1.at(0)));
  EXPECT_EQ(0, profile1.Compare(*results1.at(1)));

  // Three operations in one:
  //  - Update profile0
  //  - Remove profile1
  //  - Add profile2
  profile0.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("John"));
  update.clear();
  update.push_back(profile0);
  update.push_back(profile2);
  personal_data_->SetProfiles(&update);

  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(0, profile0.Compare(*results2.at(0)));
  EXPECT_EQ(0, profile2.Compare(*results2.at(1)));

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
  EXPECT_EQ(0, profile0.Compare(*results3.at(0)));
  EXPECT_EQ(0, profile2.Compare(*results3.at(1)));
}

// TODO(jhawkins): Test SetCreditCards w/out a WebDataService in the profile.
TEST_F(PersonalDataManagerTest, SetCreditCards) {
  CreditCard creditcard0;
  autofill_test::SetCreditCardInfo(&creditcard0, "Corporate",
      "John Dillinger", "423456789012" /* Visa */, "01", "2010");

  CreditCard creditcard1;
  autofill_test::SetCreditCardInfo(&creditcard1, "Personal",
      "Bonnie Parker", "518765432109" /* Mastercard */, "12", "2012");

  CreditCard creditcard2;
  autofill_test::SetCreditCardInfo(&creditcard2, "Savings",
      "Clyde Barrow", "347666888555" /* American Express */, "04", "2015");

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

  const std::vector<CreditCard*>& results1 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(0, creditcard0.Compare(*results1.at(0)));
  EXPECT_EQ(0, creditcard1.Compare(*results1.at(1)));

  // Three operations in one:
  //  - Update creditcard0
  //  - Remove creditcard1
  //  - Add creditcard2
  creditcard0.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Joe"));
  update.clear();
  update.push_back(creditcard0);
  update.push_back(creditcard2);
  personal_data_->SetCreditCards(&update);

  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(creditcard0, *results2.at(0));
  EXPECT_EQ(creditcard2, *results2.at(1));

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
  AutoFillProfile profile0;
  autofill_test::SetProfileInfo(&profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile profile1;
  autofill_test::SetProfileInfo(&profile1,
      "Home", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549", "13502849239");

  CreditCard creditcard0;
  autofill_test::SetCreditCardInfo(&creditcard0, "Corporate",
      "John Dillinger", "423456789012" /* Visa */, "01", "2010");

  CreditCard creditcard1;
  autofill_test::SetCreditCardInfo(&creditcard1, "Personal",
      "Bonnie Parker", "518765432109" /* Mastercard */, "12", "2012");

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

  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(0, profile0.Compare(*results1.at(0)));
  EXPECT_EQ(0, profile1.Compare(*results1.at(1)));

  MessageLoop::current()->Run();

  // Add two test credit cards to the database.
  std::vector<CreditCard> update_cc;
  update_cc.push_back(creditcard0);
  update_cc.push_back(creditcard1);
  personal_data_->SetCreditCards(&update_cc);


  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(creditcard0, *results2.at(0));
  EXPECT_EQ(creditcard1, *results2.at(1));

  // Determine uniqueness by inserting all of the GUIDs into a set and verifying
  // the size of the set matches the number of GUIDs.
  std::set<std::string> guids;
  guids.insert(profile0.guid());
  guids.insert(profile1.guid());
  guids.insert(creditcard0.guid());
  guids.insert(creditcard1.guid());
  EXPECT_EQ(4U, guids.size());
}

// Test care for 50047. Makes sure that unique_ids_ is populated correctly on
// load.
TEST_F(PersonalDataManagerTest, PopulateUniqueIDsOnLoad) {
  AutoFillProfile profile0;
  autofill_test::SetProfileInfo(&profile0,
      "", "y", "", "", "", "", "", "", "", "", "", "", "", "");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillRepeatedly(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add the profile0 to the db.
  std::vector<AutoFillProfile> update;
  update.push_back(profile0);
  personal_data_->SetProfiles(&update);

  // Reset the PersonalDataManager. This recreates PersonalDataManager, which
  // should populate unique_ids_.
  ResetPersonalDataManager();

  // The message loop will exit when the PersonalDataLoadedObserver is notified.
  MessageLoop::current()->Run();

  // Verify that we've loaded the profiles from the web database.
  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();
  ASSERT_EQ(1U, results2.size());

  // Add a new profile.
  AutoFillProfile profile1;
  autofill_test::SetProfileInfo(&profile1,
      "", "y", "", "", "", "", "", "", "", "", "", "", "", "");
  update.clear();
  update.push_back(*results2[0]);
  update.push_back(profile1);
  personal_data_->SetProfiles(&update);

  // Make sure the two profiles have different ids (and neither equal to 0,
  // which is an invalid id).
  const std::vector<AutoFillProfile*>& results3 = personal_data_->profiles();
  ASSERT_EQ(2U, results3.size());
  EXPECT_NE(results3[0]->guid(), results3[1]->guid());
  EXPECT_TRUE(guid::IsValidGUID(results3[0]->guid()));
  EXPECT_TRUE(guid::IsValidGUID(results3[1]->guid()));
}

TEST_F(PersonalDataManagerTest, SetEmptyProfile) {
  AutoFillProfile profile0;
  autofill_test::SetProfileInfo(&profile0,
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
  CreditCard creditcard0;
  autofill_test::SetCreditCardInfo(&creditcard0, "", "", "", "", "");

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
  AutoFillProfile profile0;
  autofill_test::SetProfileInfo(&profile0,
      "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile profile1;
  autofill_test::SetProfileInfo(&profile1,
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

  // Labels depend on other profiles in the list - update labels manually.
  std::vector<AutoFillProfile *> profile_pointers;
  profile_pointers.push_back(&profile0);
  profile_pointers.push_back(&profile1);
  AutoFillProfile::AdjustInferredLabels(&profile_pointers);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(2U, results1.size());
  EXPECT_EQ(profile0, *results1.at(0));
  EXPECT_EQ(profile1, *results1.at(1));

  AutoFillProfile profile2;
  autofill_test::SetProfileInfo(&profile2,
      "Work", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  // Adjust all labels.
  profile_pointers.push_back(&profile2);
  AutoFillProfile::AdjustInferredLabels(&profile_pointers);

  WebDataService* wds = profile_->GetWebDataService(Profile::EXPLICIT_ACCESS);
  ASSERT_TRUE(wds);
  wds->AddAutoFillProfileGUID(profile2);

  personal_data_->Refresh();

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
    OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();
  ASSERT_EQ(3U, results2.size());
  EXPECT_EQ(profile0, *results2.at(0));
  EXPECT_EQ(profile1, *results2.at(1));
  EXPECT_EQ(profile2, *results2.at(2));

  wds->RemoveAutoFillProfileGUID(profile1.guid());
  wds->RemoveAutoFillProfileGUID(profile2.guid());

  // Before telling the PDM to refresh, simulate an edit to one of the profiles
  // via a SetProfile update (this would happen if the AutoFill window was
  // open with a previous snapshot of the profiles, and something [e.g. sync]
  // removed a profile from the browser.  In this edge case, we will end up
  // in a consistent state by dropping the write).
  profile2.SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16("Jo"));
  update.clear();
  update.push_back(profile0);
  update.push_back(profile1);
  update.push_back(profile2);
  personal_data_->SetProfiles(&update);

  // Wait for the refresh.
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
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  AutoFillProfile expected;
  autofill_test::SetProfileInfo(&expected, NULL, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL, NULL);
  const std::vector<AutoFillProfile*>& results = personal_data_->profiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(PersonalDataManagerTest, ImportFormDataBadEmail) {
  FormData form;
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Email:", "email", "bogus", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  AutoFillProfile expected;
  autofill_test::SetProfileInfo(&expected, NULL, "George", NULL,
      "Washington", NULL, NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL, NULL);
  const std::vector<AutoFillProfile*>& results = personal_data_->profiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(PersonalDataManagerTest, ImportFormDataNotEnoughFilledFields) {
  FormData form;
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card number:", "card_number", "4111 1111 1111 1111", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure);
  const CreditCard* imported_credit_card;
  EXPECT_FALSE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  ASSERT_EQ(0U, profiles.size());
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
  ASSERT_EQ(0U, credit_cards.size());
}

TEST_F(PersonalDataManagerTest, ImportPhoneNumberSplitAcrossMultipleFields) {
  FormData form;
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Phone #:", "home_phone_area_code", "650", "text", &field);
  field.set_max_length(3);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Phone #:", "home_phone_prefix", "555", "text", &field);
  field.set_max_length(3);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Phone #:", "home_phone_suffix", "0000", "text", &field);
  field.set_max_length(4);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  AutoFillProfile expected;
  autofill_test::SetProfileInfo(&expected, NULL, "George", NULL,
      "Washington", NULL, NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, "6505550000", NULL);
  const std::vector<AutoFillProfile*>& results = personal_data_->profiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(PersonalDataManagerTest, SetUniqueCreditCardLabels) {
  CreditCard credit_card0;
  credit_card0.set_label(ASCIIToUTF16("Home"));
  credit_card0.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("John"));
  CreditCard credit_card1;
  credit_card1.set_label(ASCIIToUTF16("Home"));
  credit_card1.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Paul"));
  CreditCard credit_card2;
  credit_card2.set_label(ASCIIToUTF16("Home"));
  credit_card2.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Ringo"));
  CreditCard credit_card3;
  credit_card3.set_label(ASCIIToUTF16("NotHome"));
  credit_card3.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Other"));
  CreditCard credit_card4;
  credit_card4.set_label(ASCIIToUTF16("Work"));
  credit_card4.SetInfo(AutoFillType(CREDIT_CARD_NAME), ASCIIToUTF16("Ozzy"));
  CreditCard credit_card5;
  credit_card5.set_label(ASCIIToUTF16("Work"));
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

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  const std::vector<CreditCard*>& results = personal_data_->credit_cards();
  ASSERT_EQ(6U, results.size());
  EXPECT_EQ(ASCIIToUTF16("Home"), results[0]->Label());
  EXPECT_EQ(ASCIIToUTF16("Home2"), results[1]->Label());
  EXPECT_EQ(ASCIIToUTF16("Home3"), results[2]->Label());
  EXPECT_EQ(ASCIIToUTF16("NotHome"), results[3]->Label());
  EXPECT_EQ(ASCIIToUTF16("Work"), results[4]->Label());
  EXPECT_EQ(ASCIIToUTF16("Work2"), results[5]->Label());
}

TEST_F(PersonalDataManagerTest, AggregateTwoDifferentProfiles) {
  FormData form1;
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  AutoFillProfile expected;
  autofill_test::SetProfileInfo(&expected, NULL, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL, NULL);
  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Now create a completely different profile.
  FormData form2;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "John", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Adams", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Email:", "email", "second@gmail.com", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address:", "address1", "21 Laussat St", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.clear();
  forms.push_back(&form_structure2);
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();

  AutoFillProfile expected2;
  autofill_test::SetProfileInfo(&expected2, NULL, "John", NULL,
      "Adams", "second@gmail.com", NULL, "21 Laussat St", NULL,
      "San Francisco", "California", "94102", NULL, NULL, NULL);
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
  EXPECT_EQ(0, expected2.Compare(*results2[1]));
}

TEST_F(PersonalDataManagerTest, AggregateSameProfileWithConflict) {
  FormData form1;
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address:", "address", "1600 Pennsylvania Avenue", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address Line 2:", "address2", "Suite A", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  // Phone gets updated.
  autofill_test::CreateTestFormField(
      "Phone:", "phone", "4445556666", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  AutoFillProfile expected;
  autofill_test::SetProfileInfo(&expected, NULL, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "1600 Pennsylvania Avenue",
      "Suite A", "San Francisco", "California", "94102", NULL, "4445556666",
      NULL);
  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Now create an updated profile.
  FormData form2;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address:", "address", "1600 Pennsylvania Avenue", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address Line 2:", "address2", "Suite A", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  // Country gets added.
  autofill_test::CreateTestFormField(
      "Country:", "country", "USA", "text", &field);
  form2.fields.push_back(field);
  // Phone gets updated.
  autofill_test::CreateTestFormField(
      "Phone:", "phone", "1231231234", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.clear();
  forms.push_back(&form_structure2);
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();

  AutoFillProfile expected2;
  autofill_test::SetProfileInfo(&expected2, NULL, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "1600 Pennsylvania Avenue",
      "Suite A", "San Francisco", "California", "94102", "USA", "1231231234",
      NULL);
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateProfileWithMissingInfoInOld) {
  FormData form1;
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "Pennsylvania", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zipcode", "19106", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  AutoFillProfile expected;
  autofill_test::SetProfileInfo(&expected, NULL, "George", NULL,
      "Washington", NULL, NULL, "190 High Street", NULL,
      "Philadelphia", "Pennsylvania", "19106", NULL, NULL, NULL);
  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Submit a form with new data for the first profile.
  FormData form2;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "Philadelphia", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "Pennsylvania", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zipcode", "19106", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.clear();
  forms.push_back(&form_structure2);
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();

  AutoFillProfile expected2;
  autofill_test::SetProfileInfo(&expected2, NULL, "George", NULL,
      "Washington", "theprez@gmail.com", NULL, "190 High Street", NULL,
      "Philadelphia", "Pennsylvania", "19106", NULL, NULL, NULL);
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateProfileWithMissingInfoInNew) {
  FormData form1;
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Company:", "company", "Government", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "Pennsylvania", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zipcode", "19106", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  AutoFillProfile expected;
  autofill_test::SetProfileInfo(&expected, NULL, "George", NULL,
      "Washington", "theprez@gmail.com", "Government", "190 High Street", NULL,
      "Philadelphia", "Pennsylvania", "19106", NULL, NULL, NULL);
  const std::vector<AutoFillProfile*>& results1 = personal_data_->profiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Submit a form with new data for the first profile.
  FormData form2;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form2.fields.push_back(field);
  // Note missing Company field.
  autofill_test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "Philadelphia", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State:", "state", "Pennsylvania", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Zip:", "zipcode", "19106", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.clear();
  forms.push_back(&form_structure2);
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& results2 = personal_data_->profiles();

  // Expect no change.
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateProfileWithInsufficientAddress) {
  FormData form1;
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "First name:", "first_name", "George", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last name:", "last_name", "Washington", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Company:", "company", "Government", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Email:", "email", "theprez@gmail.com", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address Line 1:", "address", "190 High Street", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_FALSE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  const std::vector<AutoFillProfile*>& profiles = personal_data_->profiles();
  ASSERT_EQ(0U, profiles.size());
  const std::vector<CreditCard*>& credit_cards = personal_data_->credit_cards();
  ASSERT_EQ(0U, credit_cards.size());
}


TEST_F(PersonalDataManagerTest, AggregateTwoDifferentCreditCards) {
  FormData form1;

  // Start with a single valid credit card form.
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  CreditCard expected;
  autofill_test::SetCreditCardInfo(&expected,
      "L1", "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->credit_cards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card.
  FormData form2;
  autofill_test::CreateTestFormField(
      "Name on card:", "name_on_card", "Jim Johansen", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "5500 0000 0000 0004", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "02", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.clear();
  forms.push_back(&form_structure2);
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  CreditCard expected2;
  autofill_test::SetCreditCardInfo(&expected2,
      "L2", "Jim Johansen", "5500000000000004", "02", "2012");
  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(2U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
  EXPECT_EQ(0, expected2.Compare(*results2[1]));
}

TEST_F(PersonalDataManagerTest, AggregateInvalidCreditCard) {
  FormData form1;

  // Start with a single valid credit card form.
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  CreditCard expected;
  autofill_test::SetCreditCardInfo(&expected,
      "L1", "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->credit_cards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different invalid credit card.
  FormData form2;
  autofill_test::CreateTestFormField(
      "Name on card:", "name_on_card", "Jim Johansen", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "1000000000000000", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "02", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.clear();
  forms.push_back(&form_structure2);
  EXPECT_FALSE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Note: no refresh here.

  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateSameCreditCardWithConflict) {
  FormData form1;

  // Start with a single valid credit card form.
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  CreditCard expected;
  autofill_test::SetCreditCardInfo(&expected,
      "L1", "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->credit_cards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2;
  autofill_test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "4111 1111 1111 1111", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "01", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.clear();
  forms.push_back(&form_structure2);
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2012".
  CreditCard expected2;
  autofill_test::SetCreditCardInfo(&expected2,
      "L1", "Biggie Smalls", "4111111111111111", "01", "2012");
  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateEmptyCreditCardWithConflict) {
  FormData form1;

  // Start with a single valid credit card form.
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  CreditCard expected;
  autofill_test::SetCreditCardInfo(&expected,
      "L1", "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->credit_cards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second credit card with no number.
  FormData form2;
  autofill_test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "01", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2012", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.clear();
  forms.push_back(&form_structure2);
  EXPECT_FALSE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Note: no refresh here.

  // No change is expected.
  CreditCard expected2;
  autofill_test::SetCreditCardInfo(&expected2,
      "L1", "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateCreditCardWithMissingInfoInNew) {
  FormData form1;

  // Start with a single valid credit card form.
  webkit_glue::FormField field;
  autofill_test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  CreditCard expected;
  autofill_test::SetCreditCardInfo(&expected,
      "L1", "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->credit_cards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the name is missing but
  // the credit card number matches.
  FormData form2;
  // Note missing name.
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "4111111111111111", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "01", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2011", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.clear();
  forms.push_back(&form_structure2);
  EXPECT_FALSE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Note: no refresh here.

  // No change is expected.
  CreditCard expected2;
  autofill_test::SetCreditCardInfo(&expected2,
      "L1", "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(PersonalDataManagerTest, AggregateCreditCardWithMissingInfoInOld) {
  FormData form1;

  // Start with a single valid credit card form.
  webkit_glue::FormField field;
  // Note missing name.
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "01", "text", &field);
  form1.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2011", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  std::vector<const FormStructure*> forms;
  forms.push_back(&form_structure1);
  const CreditCard* imported_credit_card;
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  CreditCard expected;
  autofill_test::SetCreditCardInfo(&expected,
      "L1", NULL, "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results = personal_data_->credit_cards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2;
  autofill_test::CreateTestFormField(
      "Name on card:", "name_on_card", "Biggie Smalls", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card Number:", "card_number", "4111-1111-1111-1111", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Month:", "exp_month", "01", "text", &field);
  form2.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Exp Year:", "exp_year", "2011", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  forms.clear();
  forms.push_back(&form_structure2);
  EXPECT_TRUE(personal_data_->ImportFormData(forms, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_->SaveImportedCreditCard(*imported_credit_card);
  delete imported_credit_card;

  // Wait for the refresh.
  EXPECT_CALL(personal_data_observer_,
      OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  MessageLoop::current()->Run();

  // Expect that the newer information is saved.  In this case the year is
  // added to the existing credit card.
  CreditCard expected2;
  autofill_test::SetCreditCardInfo(&expected2,
      "L1", "Biggie Smalls", "4111111111111111", "01", "2011");
  const std::vector<CreditCard*>& results2 = personal_data_->credit_cards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}
