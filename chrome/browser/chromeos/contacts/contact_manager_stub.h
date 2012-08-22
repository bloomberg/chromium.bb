// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MANAGER_STUB_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MANAGER_STUB_H_

#include "chrome/browser/chromeos/contacts/contact_manager.h"

#include "base/memory/scoped_vector.h"

namespace contacts {

// ContactManagerInterface implementation that returns a fixed set of contacts.
// Used for testing.
class ContactManagerStub : public ContactManagerInterface {
 public:
  explicit ContactManagerStub(Profile* profile);
  virtual ~ContactManagerStub();

  // Updates |contacts_|.
  void SetContacts(const ContactPointers& contacts);

  // Invokes OnContactsUpdated() on |observers_|.
  void NotifyObserversAboutUpdatedContacts();

  // ContactManagerInterface overrides:
  virtual void AddObserver(ContactManagerObserver* observer, Profile* profile);
  virtual void RemoveObserver(ContactManagerObserver* observer,
                              Profile* profile);
  virtual scoped_ptr<ContactPointers> GetAllContacts(Profile* profile);
  virtual const Contact* GetContactById(Profile* profile,
                                        const std::string& contact_id);

 private:
  // Profile expected to be passed to ContactManagerInterface methods.
  // Passing any other profile will result in a crash.
  Profile* profile_;  // not owned

  // Observers registered for |profile_|.
  ObserverList<ContactManagerObserver> observers_;

  // Contacts that will be returned by GetAllContacts() and GetContactById().
  ScopedVector<Contact> contacts_;

  DISALLOW_COPY_AND_ASSIGN(ContactManagerStub);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MANAGER_STUB_H_
