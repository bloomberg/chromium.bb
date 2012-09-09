// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/google_contact_store.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_store_observer.h"
#include "chrome/browser/chromeos/contacts/contact_test_util.h"
#include "chrome/browser/chromeos/contacts/fake_contact_database.h"
#include "chrome/browser/chromeos/gdata/gdata_contacts_service_stub.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace contacts {
namespace test {

// ContactStoreObserver implementation that just counts the number of times
// that it's been told that a store has been updated.
class TestContactStoreObserver : public ContactStoreObserver {
 public:
  TestContactStoreObserver() : num_updates_(0) {}
  ~TestContactStoreObserver() {}

  int num_updates() const { return num_updates_; }
  void reset_stats() { num_updates_ = 0; }

  // ContactStoreObserver overrides:
  void OnContactsUpdated(ContactStore* store) OVERRIDE {
    DCHECK(store);
    num_updates_++;
  }

 private:
  // Number of times that OnContactsUpdated() has been called.
  int num_updates_;

  DISALLOW_COPY_AND_ASSIGN(TestContactStoreObserver);
};

class GoogleContactStoreTest : public testing::Test {
 public:
  GoogleContactStoreTest() : ui_thread_(BrowserThread::UI, &message_loop_) {}
  virtual ~GoogleContactStoreTest() {}

 protected:
  // testing::Test implementation.
  virtual void SetUp() OVERRIDE {
    // Create a mock NetworkChangeNotifier so the store won't be notified about
    // changes to the system's actual network state.
    network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
    profile_.reset(new TestingProfile);

    store_.reset(new GoogleContactStore(profile_.get()));
    store_->AddObserver(&observer_);

    test_api_.reset(new GoogleContactStore::TestAPI(store_.get()));

    db_ = new FakeContactDatabase;
    test_api_->SetDatabase(db_);

    gdata_service_ = new gdata::GDataContactsServiceStub;
    test_api_->SetGDataService(gdata_service_);
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  TestContactStoreObserver observer_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<GoogleContactStore> store_;
  scoped_ptr<GoogleContactStore::TestAPI> test_api_;

  FakeContactDatabase* db_;  // not owned
  gdata::GDataContactsServiceStub* gdata_service_;  // not owned

 private:
  DISALLOW_COPY_AND_ASSIGN(GoogleContactStoreTest);
};

TEST_F(GoogleContactStoreTest, LoadFromDatabase) {
  // Store two contacts in the database.
  const std::string kContactId1 = "contact1";
  const std::string kContactId2 = "contact2";
  scoped_ptr<Contact> contact1(new Contact);
  InitContact(kContactId1, "1", false, contact1.get());
  scoped_ptr<Contact> contact2(new Contact);
  InitContact(kContactId2, "2", false, contact2.get());
  ContactPointers db_contacts;
  db_contacts.push_back(contact1.get());
  db_contacts.push_back(contact2.get());
  UpdateMetadata db_metadata;
  db_->SetContacts(db_contacts, db_metadata);

  // Tell the GData service to report failure, initialize the store, and check
  // that the contacts from the database are loaded.
  gdata_service_->set_download_should_succeed(false);
  store_->Init();
  ContactPointers loaded_contacts;
  store_->AppendContacts(&loaded_contacts);
  EXPECT_EQ(VarContactsToString(2, contact1.get(), contact2.get()),
            ContactsToString(loaded_contacts));
  EXPECT_TRUE(test_api_->update_scheduled());
  EXPECT_EQ(1, observer_.num_updates());

  // Check that we can also grab the contact via its ID.
  const Contact* loaded_contact1 = store_->GetContactById(kContactId1);
  ASSERT_TRUE(loaded_contact1);
  EXPECT_EQ(ContactToString(*contact1), ContactToString(*loaded_contact1));

  // We should get NULL if we request a nonexistent contact.
  EXPECT_FALSE(store_->GetContactById("bogus_id"));
}

TEST_F(GoogleContactStoreTest, LoadFromGData) {
  // Store two contacts in the GData service.
  scoped_ptr<Contact> contact1(new Contact);
  InitContact("contact1", "1", false, contact1.get());
  scoped_ptr<Contact> contact2(new Contact);
  InitContact("contact2", "2", false, contact2.get());
  ContactPointers gdata_contacts;
  gdata_contacts.push_back(contact1.get());
  gdata_contacts.push_back(contact2.get());
  gdata_service_->SetContacts(gdata_contacts, base::Time());

  // Initialize the store and check that the contacts are loaded from GData.
  store_->Init();
  ContactPointers loaded_contacts;
  store_->AppendContacts(&loaded_contacts);
  EXPECT_EQ(VarContactsToString(2, contact1.get(), contact2.get()),
            ContactsToString(loaded_contacts));
  EXPECT_TRUE(test_api_->update_scheduled());
  EXPECT_EQ(1, observer_.num_updates());

  // The contacts should've been saved to the database, too.
  EXPECT_EQ(VarContactsToString(2, contact1.get(), contact2.get()),
            ContactMapToString(db_->contacts()));
}

TEST_F(GoogleContactStoreTest, UpdateFromGData) {
  scoped_ptr<Contact> contact1(new Contact);
  InitContact("contact1", "1", false, contact1.get());
  scoped_ptr<Contact> contact2(new Contact);
  InitContact("contact2", "2", false, contact2.get());
  scoped_ptr<Contact> contact3(new Contact);
  InitContact("contact3", "3", false, contact3.get());

  // Store the first two contacts in the database.
  ContactPointers db_contacts;
  db_contacts.push_back(contact1.get());
  db_contacts.push_back(contact2.get());
  UpdateMetadata db_metadata;
  db_->SetContacts(db_contacts, db_metadata);

  // Store all three in the GData service. We expect the update request to ask
  // for all contacts updated one millisecond after the newest contact in the
  // database.
  ContactPointers gdata_contacts;
  gdata_contacts.push_back(contact1.get());
  gdata_contacts.push_back(contact2.get());
  gdata_contacts.push_back(contact3.get());
  gdata_service_->SetContacts(
      gdata_contacts,
      base::Time::FromInternalValue(contact2->update_time()) +
      base::TimeDelta::FromMilliseconds(1));

  // Check that the store ends up with all three contacts.
  store_->Init();
  ContactPointers loaded_contacts;
  store_->AppendContacts(&loaded_contacts);
  EXPECT_EQ(VarContactsToString(
                3, contact1.get(), contact2.get(), contact3.get()),
            ContactsToString(loaded_contacts));
  EXPECT_EQ(2, observer_.num_updates());

  // All three contacts should've been saved to the database.
  EXPECT_EQ(VarContactsToString(
                3, contact1.get(), contact2.get(), contact3.get()),
            ContactMapToString(db_->contacts()));
  EXPECT_EQ(3, db_->num_saved_contacts());
  EXPECT_TRUE(test_api_->update_scheduled());
}

TEST_F(GoogleContactStoreTest, FetchUpdatedContacts) {
  scoped_ptr<Contact> contact1(new Contact);
  InitContact("contact1", "1", false, contact1.get());
  scoped_ptr<Contact> contact2(new Contact);
  InitContact("contact2", "2", false, contact2.get());
  scoped_ptr<Contact> contact3(new Contact);
  InitContact("contact3", "3", false, contact3.get());

  ContactPointers kAllContacts;
  kAllContacts.push_back(contact1.get());
  kAllContacts.push_back(contact2.get());
  kAllContacts.push_back(contact3.get());

  // Tell the GData service to return all three contacts in response to a full
  // update.
  ContactPointers gdata_contacts(kAllContacts);
  gdata_service_->SetContacts(gdata_contacts, base::Time());

  // All the contacts should be loaded and saved to the database.
  store_->Init();
  ContactPointers loaded_contacts;
  store_->AppendContacts(&loaded_contacts);
  EXPECT_EQ(ContactsToString(kAllContacts), ContactsToString(loaded_contacts));
  EXPECT_EQ(ContactsToString(kAllContacts),
            ContactMapToString(db_->contacts()));
  EXPECT_EQ(static_cast<int>(kAllContacts.size()), db_->num_saved_contacts());
  EXPECT_TRUE(test_api_->update_scheduled());
  EXPECT_EQ(1, observer_.num_updates());
  observer_.reset_stats();

  // Update the third contact.
  contact3->set_full_name("new full name");
  base::Time old_contact3_update_time =
      base::Time::FromInternalValue(contact3->update_time());
  contact3->set_update_time(
      (old_contact3_update_time +
       base::TimeDelta::FromSeconds(10)).ToInternalValue());
  gdata_contacts.clear();
  gdata_contacts.push_back(contact3.get());
  gdata_service_->SetContacts(
      gdata_contacts,
      old_contact3_update_time + base::TimeDelta::FromMilliseconds(1));

  // Check that the updated contact is loaded (i.e. the store passed the
  // correct minimum update time to the service) and saved back to the database.
  db_->reset_stats();
  test_api_->DoUpdate();
  loaded_contacts.clear();
  store_->AppendContacts(&loaded_contacts);
  EXPECT_EQ(ContactsToString(kAllContacts), ContactsToString(loaded_contacts));
  EXPECT_EQ(ContactsToString(kAllContacts),
            ContactMapToString(db_->contacts()));
  EXPECT_EQ(1, db_->num_saved_contacts());
  EXPECT_TRUE(test_api_->update_scheduled());
  EXPECT_EQ(1, observer_.num_updates());
  observer_.reset_stats();

  // The next update should be based on the third contact's new update time.
  contact3->set_full_name("yet another full name");
  gdata_service_->SetContacts(
      gdata_contacts,
      base::Time::FromInternalValue(contact3->update_time()) +
      base::TimeDelta::FromMilliseconds(1));

  db_->reset_stats();
  test_api_->DoUpdate();
  loaded_contacts.clear();
  store_->AppendContacts(&loaded_contacts);
  EXPECT_EQ(ContactsToString(kAllContacts), ContactsToString(loaded_contacts));
  EXPECT_EQ(ContactsToString(kAllContacts),
            ContactMapToString(db_->contacts()));
  EXPECT_EQ(1, db_->num_saved_contacts());
  EXPECT_TRUE(test_api_->update_scheduled());
  EXPECT_EQ(1, observer_.num_updates());
}

TEST_F(GoogleContactStoreTest, DontReturnDeletedContacts) {
  // Tell GData to return a single deleted contact.
  const std::string kContactId = "contact";
  scoped_ptr<Contact> contact(new Contact);
  InitContact(kContactId, "1", true, contact.get());
  ContactPointers gdata_contacts;
  gdata_contacts.push_back(contact.get());
  gdata_service_->SetContacts(gdata_contacts, base::Time());

  // The contact shouldn't be returned by AppendContacts() or
  // GetContactById().
  store_->Init();
  ContactPointers loaded_contacts;
  store_->AppendContacts(&loaded_contacts);
  EXPECT_TRUE(loaded_contacts.empty());
  EXPECT_FALSE(store_->GetContactById(kContactId));
}

// Test that we do a full refresh from GData if enough time has passed since the
// last refresh that we might've missed some contact deletions (see
// |kForceFullUpdateDays| in google_contact_store.cc).
TEST_F(GoogleContactStoreTest, FullRefreshAfterThirtyDays) {
  base::Time::Exploded kInitTimeExploded = { 2012, 3, 0, 1, 16, 34, 56, 123 };
  base::Time kInitTime = base::Time::FromUTCExploded(kInitTimeExploded);

  base::Time kOldUpdateTime = kInitTime - base::TimeDelta::FromDays(31);
  scoped_ptr<Contact> contact1(new Contact);
  InitContact("contact1", "1", false, contact1.get());
  contact1->set_update_time(kOldUpdateTime.ToInternalValue());
  scoped_ptr<Contact> contact2(new Contact);
  InitContact("contact2", "2", false, contact2.get());
  contact2->set_update_time(kOldUpdateTime.ToInternalValue());

  // Put both contacts in the database, along with metadata saying that the last
  // successful update was 31 days in the past.
  ContactPointers db_contacts;
  db_contacts.push_back(contact1.get());
  db_contacts.push_back(contact2.get());
  UpdateMetadata db_metadata;
  db_metadata.set_last_update_start_time(kOldUpdateTime.ToInternalValue());
  db_->SetContacts(db_contacts, db_metadata);

  // Tell the GData service to return only the first contact and to expect a
  // full refresh (since it's been a long time since the last update).
  ContactPointers gdata_contacts;
  gdata_contacts.push_back(contact1.get());
  gdata_service_->SetContacts(gdata_contacts, base::Time());

  test_api_->set_current_time(kInitTime);
  store_->Init();
  ContactPointers loaded_contacts;
  store_->AppendContacts(&loaded_contacts);
  EXPECT_EQ(ContactsToString(gdata_contacts),
            ContactsToString(loaded_contacts));
  EXPECT_TRUE(test_api_->update_scheduled());

  // Make GData return both contacts now in response to an incremental update.
  gdata_contacts.clear();
  contact2->set_update_time(kInitTime.ToInternalValue());
  gdata_contacts.push_back(contact1.get());
  gdata_contacts.push_back(contact2.get());
  gdata_service_->SetContacts(
      gdata_contacts, kOldUpdateTime + base::TimeDelta::FromMilliseconds(1));

  // Advance the time by twenty days and check that the update is successful.
  base::Time kFirstUpdateTime = kInitTime + base::TimeDelta::FromDays(20);
  test_api_->set_current_time(kFirstUpdateTime);
  test_api_->DoUpdate();
  loaded_contacts.clear();
  store_->AppendContacts(&loaded_contacts);
  EXPECT_EQ(ContactsToString(gdata_contacts),
            ContactsToString(loaded_contacts));

  // After we advance the time by 31 days, we should do a full refresh again.
  gdata_contacts.clear();
  gdata_contacts.push_back(contact1.get());
  gdata_service_->SetContacts(gdata_contacts, base::Time());
  base::Time kSecondUpdateTime =
      kFirstUpdateTime + base::TimeDelta::FromDays(31);
  test_api_->set_current_time(kSecondUpdateTime);
  test_api_->DoUpdate();
  loaded_contacts.clear();
  store_->AppendContacts(&loaded_contacts);
  EXPECT_EQ(ContactsToString(gdata_contacts),
            ContactsToString(loaded_contacts));
}

TEST_F(GoogleContactStoreTest, HandleDatabaseInitFailure) {
  scoped_ptr<Contact> contact1(new Contact);
  InitContact("contact1", "1", false, contact1.get());

  // Store a contact in the database but make initialization fail.
  ContactPointers db_contacts;
  db_contacts.push_back(contact1.get());
  UpdateMetadata db_metadata;
  db_->SetContacts(db_contacts, db_metadata);
  db_->set_init_success(false);

  // Create a second contact and tell the GData service to return it.
  scoped_ptr<Contact> contact2(new Contact);
  InitContact("contact2", "2", false, contact2.get());
  ContactPointers gdata_contacts;
  gdata_contacts.push_back(contact2.get());
  gdata_service_->SetContacts(gdata_contacts, base::Time());

  // Initialize the store. We shouldn't get the first contact (since DB
  // initialization failed) but we should still fetch the second one from GData
  // and schedule an update.
  store_->Init();
  ContactPointers loaded_contacts;
  store_->AppendContacts(&loaded_contacts);
  EXPECT_EQ(ContactsToString(gdata_contacts),
            ContactsToString(loaded_contacts));
  EXPECT_TRUE(test_api_->update_scheduled());
}

TEST_F(GoogleContactStoreTest, AvoidUpdatesWhenOffline) {
  EXPECT_EQ(0, gdata_service_->num_download_requests());

  // Notify the store that we're offline.  Init() shouldn't attempt an update
  // and the update timer shouldn't be running.
  test_api_->NotifyAboutNetworkStateChange(false);
  store_->Init();
  EXPECT_EQ(0, gdata_service_->num_download_requests());
  EXPECT_FALSE(test_api_->update_scheduled());

  // We should do an update and schedule further updates as soon as we go
  // online.
  gdata_service_->reset_stats();
  test_api_->NotifyAboutNetworkStateChange(true);
  EXPECT_EQ(1, gdata_service_->num_download_requests());
  EXPECT_TRUE(test_api_->update_scheduled());

  // If we call DoUpdate() to mimic the code path that's used for a timer-driven
  // update while we're offline, we should again defer the update.
  gdata_service_->reset_stats();
  test_api_->NotifyAboutNetworkStateChange(false);
  test_api_->DoUpdate();
  EXPECT_EQ(0, gdata_service_->num_download_requests());

  // When we're back online, the update should happen.
  gdata_service_->reset_stats();
  test_api_->NotifyAboutNetworkStateChange(true);
  EXPECT_EQ(1, gdata_service_->num_download_requests());
}

}  // namespace test
}  // namespace contacts
