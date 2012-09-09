// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/fake_contact_database.h"

#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace contacts {

FakeContactDatabase::FakeContactDatabase()
    : init_success_(true),
      save_success_(true),
      load_success_(true),
      num_saved_contacts_(0) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeContactDatabase::Init(const FilePath& database_dir,
                               InitCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  callback.Run(init_success_);
}

void FakeContactDatabase::SetContacts(const ContactPointers& contacts,
                                      const UpdateMetadata& metadata) {
  contacts_.Clear();
  MergeContacts(contacts);
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
    num_saved_contacts_ += contacts->size();
    if (is_full_update)
      contacts_.Clear();
    MergeContacts(*contacts);
    metadata_ = *metadata;
  }
  callback.Run(save_success_);
}

void FakeContactDatabase::LoadContacts(LoadCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<ScopedVector<Contact> > contacts(new ScopedVector<Contact>());
  scoped_ptr<UpdateMetadata> metadata(new UpdateMetadata);
  if (load_success_) {
    for (ContactMap::const_iterator it = contacts_.begin();
         it != contacts_.end(); ++it) {
      contacts->push_back(new Contact(*it->second));
    }
    *metadata = metadata_;
  }
  callback.Run(load_success_, contacts.Pass(), metadata.Pass());
}

FakeContactDatabase::~FakeContactDatabase() {
}

void FakeContactDatabase::MergeContacts(
    const ContactPointers& updated_contacts) {
  scoped_ptr<ScopedVector<Contact> > copied_contacts(new ScopedVector<Contact>);
  for (size_t i = 0; i < updated_contacts.size(); ++i)
    copied_contacts->push_back(new Contact(*updated_contacts[i]));
  contacts_.Merge(copied_contacts.Pass(), ContactMap::KEEP_DELETED_CONTACTS);
}

}  // namespace contacts
