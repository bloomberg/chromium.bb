// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_STORE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_STORE_OBSERVER_H_

#include "base/basictypes.h"

namespace contacts {

class ContactStore;

// Interface for observing changes to a ContactStore.
class ContactStoreObserver {
 public:
  ContactStoreObserver() {}
  virtual ~ContactStoreObserver() {}

  // Called when the contacts stored within a ContactStore are updated.
  virtual void OnContactsUpdated(ContactStore* store) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactStoreObserver);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_STORE_OBSERVER_H_
