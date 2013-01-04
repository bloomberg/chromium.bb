// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/contact_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_manager_observer.h"
#include "chrome/browser/chromeos/contacts/contact_test_util.h"
#include "chrome/browser/chromeos/contacts/fake_contact_store.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace contacts {
namespace test {

// ContactManagerObserver implementation that registers itself with a
// ContactManager and counts the number of times that it's been told that
// contacts have been updated.
class TestContactManagerObserver : public ContactManagerObserver {
 public:
  TestContactManagerObserver(ContactManager* contact_manager,
                             Profile* profile)
      : contact_manager_(contact_manager),
        profile_(profile),
        num_updates_(0) {
    contact_manager_->AddObserver(this, profile_);
  }
  ~TestContactManagerObserver() {
    contact_manager_->RemoveObserver(this, profile_);
  }

  int num_updates() const { return num_updates_; }
  void reset_stats() { num_updates_ = 0; }

  // ContactManagerObserver overrides:
  void OnContactsUpdated(Profile* profile) OVERRIDE {
    CHECK(profile == profile_);
    num_updates_++;
  }

 private:
  ContactManager* contact_manager_;  // not owned
  Profile* profile_;  // not owned

  // Number of times that OnContactsUpdated() has been called.
  int num_updates_;

  DISALLOW_COPY_AND_ASSIGN(TestContactManagerObserver);
};

class ContactManagerTest : public testing::Test {
 public:
  ContactManagerTest() : ui_thread_(BrowserThread::UI, &message_loop_) {}
  virtual ~ContactManagerTest() {}

 protected:
  // testing::Test implementation.
  virtual void SetUp() OVERRIDE {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    contact_manager_.reset(new ContactManager);
    store_factory_ = new FakeContactStoreFactory;
    contact_manager_->SetContactStoreForTesting(
        scoped_ptr<ContactStoreFactory>(store_factory_).Pass());
    contact_manager_->Init();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  scoped_ptr<TestingProfileManager> profile_manager_;
  scoped_ptr<ContactManager> contact_manager_;
  FakeContactStoreFactory* store_factory_;  // not owned

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactManagerTest);
};

TEST_F(ContactManagerTest, NotifyOnUpdate) {
  const std::string kProfileName = "test_profile";
  TestingProfile* profile =
      profile_manager_->CreateTestingProfile(kProfileName);
  TestContactManagerObserver observer(contact_manager_.get(), profile);
  EXPECT_EQ(0, observer.num_updates());

  // ContactManager should notify its observers when it receives notification
  // that a ContactStore has been updated.
  FakeContactStore* store = store_factory_->GetContactStoreForProfile(profile);
  ASSERT_TRUE(store);
  store->NotifyObserversAboutContactsUpdate();
  EXPECT_EQ(1, observer.num_updates());

  store->NotifyObserversAboutContactsUpdate();
  EXPECT_EQ(2, observer.num_updates());

  profile_manager_->DeleteTestingProfile(kProfileName);
  EXPECT_EQ(2, observer.num_updates());
}

TEST_F(ContactManagerTest, GetContacts) {
  // Create two contacts and tell the store to return them.
  const std::string kContactId1 = "1";
  scoped_ptr<Contact> contact1(new Contact);
  InitContact(kContactId1, "1", false, contact1.get());

  const std::string kContactId2 = "2";
  scoped_ptr<Contact> contact2(new Contact);
  InitContact(kContactId2, "2", false, contact2.get());

  const std::string kProfileName = "test_profile";
  TestingProfile* profile =
      profile_manager_->CreateTestingProfile(kProfileName);
  FakeContactStore* store = store_factory_->GetContactStoreForProfile(profile);
  ASSERT_TRUE(store);

  ContactPointers store_contacts;
  store_contacts.push_back(contact1.get());
  store_contacts.push_back(contact2.get());
  store->SetContacts(store_contacts);
  store->NotifyObserversAboutContactsUpdate();

  // Check that GetAllContacts() returns both contacts.
  scoped_ptr<ContactPointers> loaded_contacts =
      contact_manager_->GetAllContacts(profile);
  EXPECT_EQ(ContactsToString(store_contacts),
            ContactsToString(*loaded_contacts));

  // Check that we can get individual contacts using GetContactById().
  const Contact* loaded_contact = contact_manager_->GetContactById(profile,
                                                                   kContactId1);
  ASSERT_TRUE(loaded_contact);
  EXPECT_EQ(ContactToString(*contact1), ContactToString(*loaded_contact));

  loaded_contact = contact_manager_->GetContactById(profile, kContactId2);
  ASSERT_TRUE(loaded_contact);
  EXPECT_EQ(ContactToString(*contact2), ContactToString(*loaded_contact));

  // We should get NULL if we pass a nonexistent contact ID.
  EXPECT_FALSE(contact_manager_->GetContactById(profile, "foo"));

  profile_manager_->DeleteTestingProfile(kProfileName);
}

TEST_F(ContactManagerTest, ProfileUnsupportedByContactStore) {
  // ContactManager shouldn't try to create a ContactStore for an unsuppported
  // Profile.
  store_factory_->set_permit_store_creation(false);
  const std::string kProfileName = "test_profile";
  TestingProfile* profile =
      profile_manager_->CreateTestingProfile(kProfileName);
  EXPECT_FALSE(store_factory_->GetContactStoreForProfile(profile));
  profile_manager_->DeleteTestingProfile(kProfileName);
}

}  // namespace test
}  // namespace contacts
