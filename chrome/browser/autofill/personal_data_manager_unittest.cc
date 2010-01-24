// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    personal_data_.reset(new PersonalDataManager(profile_.get()));
    personal_data_->SetObserver(&personal_data_observer_);
  }

  static void SetProfileInfo(AutoFillProfile* profile,
      const char* label, const char* first_name, const char* middle_name,
      const char* last_name, const char* email, const char* company,
      const char* address1, const char* address2, const char* city,
      const char* state, const char* zipcode, const char* country,
      const char* phone, const char* fax) {
    profile->set_label(ASCIIToUTF16(label));
    profile->SetInfo(AutoFillType(NAME_FIRST), ASCIIToUTF16(first_name));
    profile->SetInfo(AutoFillType(NAME_MIDDLE), ASCIIToUTF16(middle_name));
    profile->SetInfo(AutoFillType(NAME_LAST), ASCIIToUTF16(last_name));
    profile->SetInfo(AutoFillType(EMAIL_ADDRESS), ASCIIToUTF16(email));
    profile->SetInfo(AutoFillType(COMPANY_NAME), ASCIIToUTF16(company));
    profile->SetInfo(AutoFillType(ADDRESS_HOME_LINE1), ASCIIToUTF16(address1));
    profile->SetInfo(AutoFillType(ADDRESS_HOME_LINE2), ASCIIToUTF16(address2));
    profile->SetInfo(AutoFillType(ADDRESS_HOME_CITY), ASCIIToUTF16(city));
    profile->SetInfo(AutoFillType(ADDRESS_HOME_STATE), ASCIIToUTF16(state));
    profile->SetInfo(AutoFillType(ADDRESS_HOME_ZIP), ASCIIToUTF16(zipcode));
    profile->SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16(country));
    profile->SetInfo(AutoFillType(PHONE_HOME_NUMBER), ASCIIToUTF16(phone));
    profile->SetInfo(AutoFillType(PHONE_FAX_NUMBER), ASCIIToUTF16(fax));
  }

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  ChromeThread db_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<PersonalDataManager> personal_data_;
  NotificationRegistrar registrar_;
  NotificationObserverMock observer_;
  PersonalDataLoadedObserverMock personal_data_observer_;
};

// TODO(jhawkins): Test SaveProfiles w/out a WebDataService in the profile.
TEST_F(PersonalDataManagerTest, SetProfiles) {
  AutoFillProfile profile0(string16(), 0);
  SetProfileInfo(&profile0, "Billing", "Marion", "Mitchell", "Morrison",
      "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5", "Hollywood", "CA",
      "91601", "US", "12345678910", "01987654321");

  AutoFillProfile profile1(string16(), 0);
  SetProfileInfo(&profile1, "Home", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "903 Apple Ct.", NULL, "Orlando", "FL", "32801",
      "US", "19482937549", "13502849239");

  AutoFillProfile profile2(string16(), 0);
  SetProfileInfo(&profile2, "Work", "Josephine", "Alicia", "Saenz",
      "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5", "Orlando", "FL",
      "32801", "US", "19482937549", "13502849239");

  // This will verify that the web database has been loaded and the notification
  // sent out.
  EXPECT_CALL(personal_data_observer_,
              OnPersonalDataLoaded()).WillOnce(QuitUIMessageLoop());

  // The message loop will exit when the mock observer is notified.
  MessageLoop::current()->Run();

  // Add the three test profiles to the database.
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
  EXPECT_EQ(profile2.unique_id(), profile1.unique_id());

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
