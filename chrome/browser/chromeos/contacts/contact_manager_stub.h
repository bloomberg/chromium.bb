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
  virtual base::WeakPtr<ContactManagerInterface> GetWeakPtr() OVERRIDE;
  virtual void AddObserver(ContactManagerObserver* observer, Profile* profile)
      OVERRIDE;
  virtual void RemoveObserver(ContactManagerObserver* observer,
                              Profile* profile) OVERRIDE;
  virtual scoped_ptr<ContactPointers> GetAllContacts(Profile* profile) OVERRIDE;
  virtual const Contact* GetContactById(Profile* profile,
                                        const std::string& contact_id) OVERRIDE;

 private:
  // Profile expected to be passed to ContactManagerInterface methods.
  // Passing any other profile will result in a crash.
  Profile* profile_;  // not owned

  // Observers registered for |profile_|.
  ObserverList<ContactManagerObserver> observers_;

  // Contacts that will be returned by GetAllContacts() and GetContactById().
  ScopedVector<Contact> contacts_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ContactManagerInterface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContactManagerStub);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MANAGER_STUB_H_
