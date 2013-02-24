// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/contact_database.h"

#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"

using content::BrowserThread;

namespace contacts {
namespace test {

// Name of the directory created within a temporary directory to store the
// contacts database.
const base::FilePath::CharType kDatabaseDirectoryName[] =
    FILE_PATH_LITERAL("contacts");

class ContactDatabaseTest : public testing::Test {
 public:
  ContactDatabaseTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_(NULL) {
  }

  virtual ~ContactDatabaseTest() {
  }

 protected:
  // testing::Test implementation.
  virtual void SetUp() OVERRIDE {
    CHECK(temp_dir_.CreateUniqueTempDir());
    CreateDatabase();
  }

  virtual void TearDown() OVERRIDE {
    DestroyDatabase();
  }

 protected:
  base::FilePath database_path() const {
    return temp_dir_.path().Append(kDatabaseDirectoryName);
  }

  void CreateDatabase() {
    DestroyDatabase();
    db_ = new ContactDatabase;
    db_->Init(database_path(),
              base::Bind(&ContactDatabaseTest::OnDatabaseInitialized,
                         base::Unretained(this)));

    // The database will be initialized on the file thread; run the message loop
    // until that happens.
    message_loop_.Run();
  }

  void DestroyDatabase() {
    if (db_) {
      db_->DestroyOnUIThread();
      db_ = NULL;
    }
  }

  // Calls ContactDatabase::SaveContacts() and blocks until the operation is
  // complete.
  void SaveContacts(scoped_ptr<ContactPointers> contacts_to_save,
                    scoped_ptr<ContactDatabaseInterface::ContactIds>
                        contact_ids_to_delete,
                    scoped_ptr<UpdateMetadata> metadata,
                    bool is_full_update) {
    CHECK(db_);
    db_->SaveContacts(contacts_to_save.Pass(),
                      contact_ids_to_delete.Pass(),
                      metadata.Pass(),
                      is_full_update,
                      base::Bind(&ContactDatabaseTest::OnContactsSaved,
                                 base::Unretained(this)));
    message_loop_.Run();
  }

  // Calls ContactDatabase::LoadContacts() and blocks until the operation is
  // complete.
  void LoadContacts(scoped_ptr<ScopedVector<Contact> >* contacts_out,
                    scoped_ptr<UpdateMetadata>* metadata_out) {
    CHECK(db_);
    db_->LoadContacts(base::Bind(&ContactDatabaseTest::OnContactsLoaded,
                                 base::Unretained(this)));
    message_loop_.Run();
    contacts_out->swap(loaded_contacts_);
    metadata_out->swap(loaded_metadata_);
  }

 private:
  void OnDatabaseInitialized(bool success) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    CHECK(success);
    // TODO(derat): Move google_apis::test::RunBlockingPoolTask() to a shared
    // location and use it for these tests.
    message_loop_.Quit();
  }

  void OnContactsSaved(bool success) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    CHECK(success);
    message_loop_.Quit();
  }

  void OnContactsLoaded(bool success,
                        scoped_ptr<ScopedVector<Contact> > contacts,
                        scoped_ptr<UpdateMetadata> metadata) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    CHECK(success);
    loaded_contacts_.swap(contacts);
    loaded_metadata_.swap(metadata);
    message_loop_.Quit();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  // Temporary directory where the database is saved.
  base::ScopedTempDir temp_dir_;

  // This class retains ownership of this object.
  ContactDatabase* db_;

  // Contacts and metadata returned by the most-recent
  // ContactDatabase::LoadContacts() call.  Used to pass returned values from
  // OnContactsLoaded() to LoadContacts().
  scoped_ptr<ScopedVector<Contact> > loaded_contacts_;
  scoped_ptr<UpdateMetadata> loaded_metadata_;

  DISALLOW_COPY_AND_ASSIGN(ContactDatabaseTest);
};

TEST_F(ContactDatabaseTest, SaveAndReload) {
  // Save a contact to the database and check that we get the same data back
  // when loading it.
  const std::string kContactId = "contact_id_1";
  scoped_ptr<Contact> contact(new Contact);
  InitContact(kContactId, "1", false, contact.get());
  AddEmailAddress("email_1", Contact_AddressType_Relation_HOME,
                  "email_label_1", true, contact.get());
  AddEmailAddress("email_2", Contact_AddressType_Relation_WORK,
                  "", false, contact.get());
  AddPhoneNumber("123-456-7890", Contact_AddressType_Relation_HOME,
                 "phone_label", true, contact.get());
  AddPostalAddress("postal_1", Contact_AddressType_Relation_HOME,
                   "postal_label_1", true, contact.get());
  AddPostalAddress("postal_2", Contact_AddressType_Relation_OTHER,
                   "postal_label_2", false, contact.get());
  AddInstantMessagingAddress("im_1",
                             Contact_InstantMessagingAddress_Protocol_AIM,
                             Contact_AddressType_Relation_HOME,
                             "im_label_1", true, contact.get());
  SetPhoto(gfx::Size(20, 20), contact.get());
  scoped_ptr<ContactPointers> contacts_to_save(new ContactPointers);
  contacts_to_save->push_back(contact.get());
  scoped_ptr<ContactDatabaseInterface::ContactIds> contact_ids_to_delete(
      new ContactDatabaseInterface::ContactIds);

  const int64 kLastUpdateTime = 1234;
  scoped_ptr<UpdateMetadata> metadata_to_save(new UpdateMetadata);
  metadata_to_save->set_last_update_start_time(kLastUpdateTime);

  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               true);
  scoped_ptr<ScopedVector<Contact> > loaded_contacts;
  scoped_ptr<UpdateMetadata> loaded_metadata;
  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_EQ(VarContactsToString(1, contact.get()),
            ContactsToString(*loaded_contacts));
  EXPECT_EQ(kLastUpdateTime, loaded_metadata->last_update_start_time());

  // Modify the contact, save it, and check that the loaded contact is also
  // updated.
  InitContact(kContactId, "2", false, contact.get());
  AddEmailAddress("email_3", Contact_AddressType_Relation_OTHER,
                  "email_label_2", true, contact.get());
  AddPhoneNumber("phone_2", Contact_AddressType_Relation_OTHER,
                 "phone_label_2", false, contact.get());
  AddPostalAddress("postal_3", Contact_AddressType_Relation_HOME,
                   "postal_label_3", true, contact.get());
  SetPhoto(gfx::Size(64, 64), contact.get());
  contacts_to_save.reset(new ContactPointers);
  contacts_to_save->push_back(contact.get());
  contact_ids_to_delete.reset(new ContactDatabaseInterface::ContactIds);
  metadata_to_save.reset(new UpdateMetadata);
  const int64 kNewLastUpdateTime = 5678;
  metadata_to_save->set_last_update_start_time(kNewLastUpdateTime);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               true);

  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_EQ(VarContactsToString(1, contact.get()),
            ContactsToString(*loaded_contacts));
  EXPECT_EQ(kNewLastUpdateTime, loaded_metadata->last_update_start_time());
}

TEST_F(ContactDatabaseTest, FullAndIncrementalUpdates) {
  // Do a full update that inserts two contacts into the database.
  const std::string kContactId1 = "contact_id_1";
  const std::string kSharedEmail = "foo@example.org";
  scoped_ptr<Contact> contact1(new Contact);
  InitContact(kContactId1, "1", false, contact1.get());
  AddEmailAddress(kSharedEmail, Contact_AddressType_Relation_HOME,
                  "", true, contact1.get());

  const std::string kContactId2 = "contact_id_2";
  scoped_ptr<Contact> contact2(new Contact);
  InitContact(kContactId2, "2", false, contact2.get());
  AddEmailAddress(kSharedEmail, Contact_AddressType_Relation_WORK,
                  "", true, contact2.get());

  scoped_ptr<ContactPointers> contacts_to_save(new ContactPointers);
  contacts_to_save->push_back(contact1.get());
  contacts_to_save->push_back(contact2.get());
  scoped_ptr<ContactDatabaseInterface::ContactIds> contact_ids_to_delete(
      new ContactDatabaseInterface::ContactIds);
  scoped_ptr<UpdateMetadata> metadata_to_save(new UpdateMetadata);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               true);

  scoped_ptr<ScopedVector<Contact> > loaded_contacts;
  scoped_ptr<UpdateMetadata> loaded_metadata;
  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_EQ(VarContactsToString(2, contact1.get(), contact2.get()),
            ContactsToString(*loaded_contacts));

  // Do an incremental update including just the second contact.
  InitContact(kContactId2, "2b", false, contact2.get());
  AddPostalAddress("postal_1", Contact_AddressType_Relation_HOME,
                   "", true, contact2.get());
  contacts_to_save.reset(new ContactPointers);
  contacts_to_save->push_back(contact2.get());
  contact_ids_to_delete.reset(new ContactDatabaseInterface::ContactIds);
  metadata_to_save.reset(new UpdateMetadata);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               false);
  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_EQ(VarContactsToString(2, contact1.get(), contact2.get()),
            ContactsToString(*loaded_contacts));

  // Do an empty incremental update and check that the metadata is still
  // updated.
  contacts_to_save.reset(new ContactPointers);
  contact_ids_to_delete.reset(new ContactDatabaseInterface::ContactIds);
  metadata_to_save.reset(new UpdateMetadata);
  const int64 kLastUpdateTime = 1234;
  metadata_to_save->set_last_update_start_time(kLastUpdateTime);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               false);
  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_EQ(VarContactsToString(2, contact1.get(), contact2.get()),
            ContactsToString(*loaded_contacts));
  EXPECT_EQ(kLastUpdateTime, loaded_metadata->last_update_start_time());

  // Do a full update including just the first contact.  The second contact
  // should be removed from the database.
  InitContact(kContactId1, "1b", false, contact1.get());
  AddPostalAddress("postal_2", Contact_AddressType_Relation_WORK,
                   "", true, contact1.get());
  AddPhoneNumber("phone", Contact_AddressType_Relation_HOME,
                 "", true, contact1.get());
  contacts_to_save.reset(new ContactPointers);
  contacts_to_save->push_back(contact1.get());
  contact_ids_to_delete.reset(new ContactDatabaseInterface::ContactIds);
  metadata_to_save.reset(new UpdateMetadata);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               true);
  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_EQ(VarContactsToString(1, contact1.get()),
            ContactsToString(*loaded_contacts));

  // Do a full update including no contacts.  The database should be cleared.
  contacts_to_save.reset(new ContactPointers);
  contact_ids_to_delete.reset(new ContactDatabaseInterface::ContactIds);
  metadata_to_save.reset(new UpdateMetadata);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               true);
  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_TRUE(loaded_contacts->empty());
}

// Test that we create a new database when we encounter a corrupted one.
TEST_F(ContactDatabaseTest, DeleteWhenCorrupt) {
  DestroyDatabase();
  // Overwrite all of the files in the database with a space character.
  file_util::FileEnumerator enumerator(
      database_path(), false, file_util::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    file_util::WriteFile(path, " ", 1);
  }
  CreateDatabase();

  // Make sure that the resulting database is usable.
  scoped_ptr<Contact> contact(new Contact);
  InitContact("1", "1", false, contact.get());
  scoped_ptr<ContactPointers> contacts_to_save(new ContactPointers);
  contacts_to_save->push_back(contact.get());
  scoped_ptr<ContactDatabaseInterface::ContactIds> contact_ids_to_delete(
      new ContactDatabaseInterface::ContactIds);
  scoped_ptr<UpdateMetadata> metadata_to_save(new UpdateMetadata);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               true);

  scoped_ptr<ScopedVector<Contact> > loaded_contacts;
  scoped_ptr<UpdateMetadata> loaded_metadata;
  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_EQ(VarContactsToString(1, contact.get()),
            ContactsToString(*loaded_contacts));
}

TEST_F(ContactDatabaseTest, DeleteRequestedContacts) {
  // Insert two contacts into the database with a full update.
  const std::string kContactId1 = "contact_id_1";
  scoped_ptr<Contact> contact1(new Contact);
  InitContact(kContactId1, "1", false, contact1.get());
  const std::string kContactId2 = "contact_id_2";
  scoped_ptr<Contact> contact2(new Contact);
  InitContact(kContactId2, "2", false, contact2.get());

  scoped_ptr<ContactPointers> contacts_to_save(new ContactPointers);
  contacts_to_save->push_back(contact1.get());
  contacts_to_save->push_back(contact2.get());
  scoped_ptr<ContactDatabaseInterface::ContactIds> contact_ids_to_delete(
      new ContactDatabaseInterface::ContactIds);
  scoped_ptr<UpdateMetadata> metadata_to_save(new UpdateMetadata);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               true);

  // Do an incremental update that inserts a third contact and deletes the first
  // contact.
  const std::string kContactId3 = "contact_id_3";
  scoped_ptr<Contact> contact3(new Contact);
  InitContact(kContactId3, "3", false, contact3.get());

  contacts_to_save.reset(new ContactPointers);
  contacts_to_save->push_back(contact3.get());
  contact_ids_to_delete.reset(new ContactDatabaseInterface::ContactIds);
  contact_ids_to_delete->push_back(kContactId1);
  metadata_to_save.reset(new UpdateMetadata);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               false);

  // LoadContacts() should return only the second and third contacts.
  scoped_ptr<ScopedVector<Contact> > loaded_contacts;
  scoped_ptr<UpdateMetadata> loaded_metadata;
  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_EQ(VarContactsToString(2, contact2.get(), contact3.get()),
            ContactsToString(*loaded_contacts));

  // Do another incremental update that deletes the second contact.
  contacts_to_save.reset(new ContactPointers);
  contact_ids_to_delete.reset(new ContactDatabaseInterface::ContactIds);
  contact_ids_to_delete->push_back(kContactId2);
  metadata_to_save.reset(new UpdateMetadata);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               false);
  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_EQ(VarContactsToString(1, contact3.get()),
            ContactsToString(*loaded_contacts));

  // Deleting a contact that isn't present should be a no-op.
  contacts_to_save.reset(new ContactPointers);
  contact_ids_to_delete.reset(new ContactDatabaseInterface::ContactIds);
  contact_ids_to_delete->push_back("bogus_id");
  metadata_to_save.reset(new UpdateMetadata);
  SaveContacts(contacts_to_save.Pass(),
               contact_ids_to_delete.Pass(),
               metadata_to_save.Pass(),
               false);
  LoadContacts(&loaded_contacts, &loaded_metadata);
  EXPECT_EQ(VarContactsToString(1, contact3.get()),
            ContactsToString(*loaded_contacts));
}

}  // namespace test
}  // namespace contacts
