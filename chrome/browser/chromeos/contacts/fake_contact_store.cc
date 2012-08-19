// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/fake_contact_store.h"

#include <utility>

#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_store_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace contacts {

FakeContactStore::FakeContactStore(FakeContactStoreFactory* factory)
    : factory_(factory),
      contacts_deleter_(&contacts_) {
}

FakeContactStore::~FakeContactStore() {
  factory_->RemoveStore(this);
}

void FakeContactStore::SetContacts(const ContactPointers& contacts) {
  STLDeleteValues(&contacts_);
  contacts_.clear();
  for (ContactPointers::const_iterator it = contacts.begin();
       it != contacts.end(); ++it) {
    contacts_[(*it)->contact_id()] = new Contact(**it);
  }
}

void FakeContactStore::NotifyObserversAboutContactsUpdate() {
  FOR_EACH_OBSERVER(ContactStoreObserver,
                    observers_,
                    OnContactsUpdated(this));
}

void FakeContactStore::Init() {
}

void FakeContactStore::AppendContacts(ContactPointers* contacts_out) {
  CHECK(contacts_out);
  for (ContactMap::const_iterator it = contacts_.begin();
       it != contacts_.end(); ++it) {
    if (!it->second->deleted())
      contacts_out->push_back(it->second);
  }
}

const Contact* FakeContactStore::GetContactById(const std::string& contact_id) {
  ContactMap::const_iterator it = contacts_.find(contact_id);
  return (it != contacts_.end() && !it->second->deleted()) ? it->second : NULL;
}

void FakeContactStore::AddObserver(ContactStoreObserver* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void FakeContactStore::RemoveObserver(ContactStoreObserver* observer) {
  CHECK(observer);
  observers_.RemoveObserver(observer);
}

FakeContactStoreFactory::FakeContactStoreFactory()
    : permit_store_creation_(true) {
}

FakeContactStoreFactory::~FakeContactStoreFactory() {
  CHECK(stores_.empty());
}

FakeContactStore* FakeContactStoreFactory::GetContactStoreForProfile(
    Profile* profile) {
  CHECK(profile);
  ProfileStoreMap::const_iterator it = stores_.find(profile);
  return it != stores_.end() ? it->second : NULL;
}

void FakeContactStoreFactory::RemoveStore(FakeContactStore* store) {
  CHECK(store);
  for (ProfileStoreMap::iterator it = stores_.begin();
       it != stores_.end(); ++it) {
    if (it->second == store) {
      stores_.erase(it);
      return;
    }
  }
  NOTREACHED() << "No record of destroyed FakeContactStore " << store;
}

bool FakeContactStoreFactory::CanCreateContactStoreForProfile(
    Profile* profile) {
  CHECK(profile);
  return permit_store_creation_;
}

ContactStore* FakeContactStoreFactory::CreateContactStore(Profile* profile) {
  CHECK(profile);
  CHECK(CanCreateContactStoreForProfile(profile));
  FakeContactStore* store = new FakeContactStore(this);
  CHECK(stores_.insert(std::make_pair(profile, store)).second)
      << "Got request to create second FakeContactStore for profile "
      << profile << " (" << profile->GetProfileName() << ")";
  return store;
}

}  // namespace contacts
