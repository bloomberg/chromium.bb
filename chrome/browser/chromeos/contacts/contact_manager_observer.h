// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MANAGER_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MANAGER_OBSERVER_H_

class Profile;

namespace contacts {

class ContactManager;

// Interface for classes that need to observe changes to ContactManager.
class ContactManagerObserver {
 public:
  ContactManagerObserver() {}
  virtual ~ContactManagerObserver() {}

  // Called when |profile|'s contacts have been updated.
  virtual void OnContactsUpdated(Profile* profile) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactManagerObserver);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MANAGER_OBSERVER_H_
