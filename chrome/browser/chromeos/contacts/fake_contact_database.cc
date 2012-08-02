// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/fake_contact_database.h"

#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_test_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace contacts {

FakeContactDatabase::FakeContactDatabase()
    : init_success_(true),
      save_success_(true),
      load_success_(true) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeContactDatabase::Init(const FilePath& database_dir,
                               InitCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(init_success_);
}

void FakeContactDatabase::SetContacts(const ContactPointers& contacts,
                                      const UpdateMetadata& metadata) {
  test::CopyContacts(contacts, &contacts_);
  metadata_ = metadata;
}

void FakeContactDatabase::DestroyOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  delete this;
}

void FakeContactDatabase::SaveContacts(scoped_ptr<ContactPointers> contacts,
                                       scoped_ptr<UpdateMetadata> metadata,
                                       bool is_full_update,
                                       SaveCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (save_success_) {
    test::CopyContacts(*contacts, &contacts_);
    metadata_ = *metadata;
  }
  callback.Run(save_success_);
}

void FakeContactDatabase::LoadContacts(LoadCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ScopedVector<Contact> > contacts(new ScopedVector<Contact>());
  scoped_ptr<UpdateMetadata> metadata(new UpdateMetadata);
  if (load_success_) {
    test::CopyContacts(contacts_, contacts.get());
    *metadata = metadata_;
  }
  callback.Run(load_success_, contacts.Pass(), metadata.Pass());
}

FakeContactDatabase::~FakeContactDatabase() {
}

}  // namespace contacts
