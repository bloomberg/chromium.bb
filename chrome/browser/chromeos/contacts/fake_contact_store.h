// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_FAKE_CONTACT_STORE_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_FAKE_CONTACT_STORE_H_

#include "chrome/browser/chromeos/contacts/contact_store.h"

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/stl_util.h"

class Profile;

namespace contacts {

class Contact;
class FakeContactStoreFactory;
typedef std::vector<const Contact*> ContactPointers;

// A "fake" in-memory implementation of ContactStore used for testing.
class FakeContactStore : public ContactStore {
 public:
  explicit FakeContactStore(FakeContactStoreFactory* factory);
  virtual ~FakeContactStore();

  // Makes an internal copy of |contacts| so they can be returned by
  // AppendContacts() and GetContactById().
  void SetContacts(const ContactPointers& contacts);

  // Invokes observers' OnContactsUpdated() methods.
  void NotifyObserversAboutContactsUpdate();

  // ContactStore implementation:
  virtual void Init() OVERRIDE;
  virtual void AppendContacts(ContactPointers* contacts_out) OVERRIDE;
  virtual const Contact* GetContactById(const std::string& contact_id) OVERRIDE;
  virtual void AddObserver(ContactStoreObserver* observer) OVERRIDE;
  virtual void RemoveObserver(ContactStoreObserver* observer) OVERRIDE;

 private:
  // Map from a contact's ID to the contact itself.
  typedef std::map<std::string, Contact*> ContactMap;

  // Factory that created this store.  Not owned.
  FakeContactStoreFactory* factory_;

  ObserverList<ContactStoreObserver> observers_;

  // Owns the pointed-to Contact values.
  ContactMap contacts_;

  // Deletes values in |contacts_|.
  STLValueDeleter<ContactMap> contacts_deleter_;

  DISALLOW_COPY_AND_ASSIGN(FakeContactStore);
};

// ContactStoreFactory implementation that returns FakeContactStores.
class FakeContactStoreFactory : public ContactStoreFactory {
 public:
  FakeContactStoreFactory();
  virtual ~FakeContactStoreFactory();

  void set_permit_store_creation(bool permit) {
    permit_store_creation_ = permit;
  }

  // Returns the FakeContactStore previously created for |profile|, or NULL if
  // no store has been created for it.
  FakeContactStore* GetContactStoreForProfile(Profile* profile);

  // Removes |store| from |stores_| after being called by a FakeContactStore's
  // d'tor.
  void RemoveStore(FakeContactStore* store);

  // ContactStoreFactory implementation:
  virtual bool CanCreateContactStoreForProfile(Profile* profile) OVERRIDE;
  virtual ContactStore* CreateContactStore(Profile* profile) OVERRIDE;

 private:
  typedef std::map<Profile*, FakeContactStore*> ProfileStoreMap;

  // Live FakeContactStore objects that we've handed out.  We don't retain
  // ownership of these, but we hang on to the pointers so that tests can
  // manipulate the stores while they're in use.
  ProfileStoreMap stores_;

  // Should CanCreateContactStoreForProfile() return true?
  bool permit_store_creation_;

  DISALLOW_COPY_AND_ASSIGN(FakeContactStoreFactory);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_FAKE_CONTACT_STORE_H_
