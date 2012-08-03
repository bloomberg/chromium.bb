// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_STORE_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_STORE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace contacts {

class Contact;
typedef std::vector<const Contact*> ContactPointers;
class ContactStoreObserver;

// Interface for classes that store contacts from a particular source.
class ContactStore {
 public:
  ContactStore() {}
  virtual ~ContactStore() {}

  // Appends all (non-deleted) contacts to |contacts_out|.
  virtual void AppendContacts(ContactPointers* contacts_out) = 0;

  // Returns the contact identified by |provider_id|.
  // NULL is returned if the contact doesn't exist.
  virtual const Contact* GetContactByProviderId(
      const std::string& provider_id) = 0;

  // Adds or removes an observer.
  virtual void AddObserver(ContactStoreObserver* observer) = 0;
  virtual void RemoveObserver(ContactStoreObserver* observer) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactStore);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_STORE_H_
