// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/contact_manager_stub.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_manager_observer.h"
#include "chrome/browser/chromeos/contacts/contact_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace contacts {

ContactManagerStub::ContactManagerStub(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {
}

ContactManagerStub::~ContactManagerStub() {}

void ContactManagerStub::NotifyObserversAboutUpdatedContacts() {
  FOR_EACH_OBSERVER(ContactManagerObserver,
                    observers_,
                    OnContactsUpdated(profile_));
}

void ContactManagerStub::SetContacts(const ContactPointers& contacts) {
  test::CopyContacts(contacts, &contacts_);
}

base::WeakPtr<ContactManagerInterface> ContactManagerStub::GetWeakPtr() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return weak_ptr_factory_.GetWeakPtr();
}

void ContactManagerStub::AddObserver(ContactManagerObserver* observer,
                                     Profile* profile) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(observer);
  CHECK_EQ(profile, profile_);
  CHECK(!observers_.HasObserver(observer));
  observers_.AddObserver(observer);
}

void ContactManagerStub::RemoveObserver(ContactManagerObserver* observer,
                                        Profile* profile) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(observer);
  CHECK_EQ(profile, profile_);
  CHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

scoped_ptr<ContactPointers> ContactManagerStub::GetAllContacts(
    Profile* profile) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK_EQ(profile, profile_);
  scoped_ptr<ContactPointers> contacts(new ContactPointers);
  for (size_t i = 0; i < contacts_.size(); ++i)
    contacts->push_back(contacts_[i]);
  return contacts.Pass();
}

const Contact* ContactManagerStub::GetContactById(
    Profile* profile,
    const std::string& contact_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK_EQ(profile, profile_);
  for (size_t i = 0; i < contacts_.size(); ++i) {
    if (contacts_[i]->contact_id() == contact_id)
      return contacts_[i];
  }
  return NULL;
}

}  // namespace contacts
