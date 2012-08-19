// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_STORE_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_STORE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

class Profile;

namespace contacts {

class Contact;
typedef std::vector<const Contact*> ContactPointers;
class ContactStoreObserver;

// Interface for classes that store contacts from a particular source.
class ContactStore {
 public:
  ContactStore() {}
  virtual ~ContactStore() {}

  // Initializes the object.
  virtual void Init() = 0;

  // Appends all (non-deleted) contacts to |contacts_out|.
  virtual void AppendContacts(ContactPointers* contacts_out) = 0;

  // Returns the contact identified by |contact_id|.
  // NULL is returned if the contact doesn't exist.
  virtual const Contact* GetContactById(const std::string& contact_id) = 0;

  // Adds or removes an observer.
  virtual void AddObserver(ContactStoreObserver* observer) = 0;
  virtual void RemoveObserver(ContactStoreObserver* observer) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactStore);
};

// Interface for factories that return ContactStore objects of a given type.
class ContactStoreFactory {
 public:
  ContactStoreFactory() {}
  virtual ~ContactStoreFactory() {}

  // Does |profile| support this type of ContactStore?
  virtual bool CanCreateContactStoreForProfile(Profile* profile) = 0;

  // Creates and returns a new, uninitialized ContactStore for |profile|.
  // CanCreateContactStoreForProfile() should be called first.
  // TODO(derat): Figure out how this should work if/when there are other
  // ContactStore implementations that need additional information beyond the
  // stuff contained in a Profile.
  virtual ContactStore* CreateContactStore(Profile* profile) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactStoreFactory);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_STORE_H_
