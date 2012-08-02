// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_FAKE_CONTACT_DATABASE_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_FAKE_CONTACT_DATABASE_H_

#include "chrome/browser/chromeos/contacts/contact_database.h"

#include "chrome/browser/chromeos/contacts/contact.pb.h"

namespace contacts {

// Fake implementation used for testing.
class FakeContactDatabase : public ContactDatabaseInterface {
 public:
  FakeContactDatabase();

  const ScopedVector<Contact>& contacts() const { return contacts_; }

  void set_init_success(bool success) { init_success_ = success; }
  void set_save_success(bool success) { save_success_ = success; }
  void set_load_success(bool success) { load_success_ = success; }

  // Copies |contacts| into |contacts_| and |metadata| into |metadata_|.  These
  // values will be returned by subsequent calls to LoadContacts().
  void SetContacts(const ContactPointers& contacts,
                   const UpdateMetadata& metadata);

  // ContactDatabaseInterface implementation.
  virtual void DestroyOnUIThread() OVERRIDE;
  virtual void Init(const FilePath& database_dir,
                    InitCallback callback) OVERRIDE;
  virtual void SaveContacts(scoped_ptr<ContactPointers> contacts,
                            scoped_ptr<UpdateMetadata> metadata,
                            bool is_full_update,
                            SaveCallback callback) OVERRIDE;
  virtual void LoadContacts(LoadCallback callback) OVERRIDE;

 protected:
  virtual ~FakeContactDatabase();

 private:
  // Should we report success in response to various requests?
  bool init_success_;
  bool save_success_;
  bool load_success_;

  // Currently-stored contacts and metadata.
  ScopedVector<Contact> contacts_;
  UpdateMetadata metadata_;

  DISALLOW_COPY_AND_ASSIGN(FakeContactDatabase);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_FAKE_CONTACT_DATABASE_H_
