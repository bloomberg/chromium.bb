// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MANAGER_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/contacts/contact_store_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace contacts {

class Contact;
typedef std::vector<const Contact*> ContactPointers;

class ContactManagerObserver;
class ContactStore;
class ContactStoreFactory;

// Class that exposes contacts to rest of the browser.
class ContactManagerInterface {
 public:
  ContactManagerInterface() {}
  virtual ~ContactManagerInterface() {}

  // Returns a weak pointer tied to the lifetime of this object.
  virtual base::WeakPtr<ContactManagerInterface> GetWeakPtr() = 0;

  // Adds or removes an observer for changes to |profile|'s contacts.
  virtual void AddObserver(ContactManagerObserver* observer,
                           Profile* profile) = 0;
  virtual void RemoveObserver(ContactManagerObserver* observer,
                              Profile* profile) = 0;

  // Returns pointers to all currently-loaded contacts for |profile|.  The
  // returned Contact objects may not persist indefinitely; the caller must not
  // refer to them again after unblocking the UI thread.
  virtual scoped_ptr<ContactPointers> GetAllContacts(Profile* profile) = 0;

  // Returns the contact identified by |contact_id|.
  // NULL is returned if the contact doesn't exist.
  virtual const Contact* GetContactById(Profile* profile,
                                        const std::string& contact_id) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactManagerInterface);
};

// Real, singleton implementation of ContactManagerInterface.
class ContactManager : public ContactManagerInterface,
                       public ContactStoreObserver,
                       public content::NotificationObserver {
 public:
  static ContactManager* GetInstance();

  ContactManager();
  virtual ~ContactManager();

  // Swaps in a new factory to use for creating ContactStores.
  // Must be called before any stores have been created.
  void SetContactStoreForTesting(scoped_ptr<ContactStoreFactory> factory);

  void Init();

  // ContactManagerInterface overrides:
  virtual base::WeakPtr<ContactManagerInterface> GetWeakPtr() OVERRIDE;
  virtual void AddObserver(ContactManagerObserver* observer,
                           Profile* profile) OVERRIDE;
  virtual void RemoveObserver(ContactManagerObserver* observer,
                              Profile* profile) OVERRIDE;
  virtual scoped_ptr<ContactPointers> GetAllContacts(Profile* profile) OVERRIDE;
  virtual const Contact* GetContactById(Profile* profile,
                                        const std::string& contact_id) OVERRIDE;

  // ContactStoreObserver overrides:
  virtual void OnContactsUpdated(ContactStore* store) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  typedef ObserverList<ContactManagerObserver> Observers;
  typedef std::map<Profile*, ContactStore*> ContactStoreMap;
  typedef std::map<Profile*, Observers*> ProfileObserversMap;

  // Returns the list of observers interested in |profile|.  If not present,
  // creates a new list if |create| is true and returns NULL otherwise.
  Observers* GetObserversForProfile(Profile* profile, bool create);

  // Handles profile creation and destruction.
  void HandleProfileCreated(Profile* profile);
  void HandleProfileDestroyed(Profile* profile);

  content::NotificationRegistrar registrar_;

  // Maps from a profile to observers that are interested in changes to that
  // profile's contacts.
  ProfileObserversMap profile_observers_;

  // Deletes values in |profile_observers_|.
  STLValueDeleter<ProfileObserversMap> profile_observers_deleter_;

  // Creates objects for |contact_stores_|.
  scoped_ptr<ContactStoreFactory> contact_store_factory_;

  // Maps from a profile to a store for getting the profile's contacts.
  ContactStoreMap contact_stores_;

  // Deletes values in |contact_stores_|.
  STLValueDeleter<ContactStoreMap> contact_stores_deleter_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ContactManagerInterface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContactManager);
};

}  // namespace contacts

#endif  // CHROME_BROWSER_CHROMEOS_CONTACTS_CONTACT_MANAGER_H_
