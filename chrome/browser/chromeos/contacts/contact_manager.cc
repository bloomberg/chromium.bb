// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/contacts/contact_manager.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/contacts/contact.pb.h"
#include "chrome/browser/chromeos/contacts/contact_manager_observer.h"
#include "chrome/browser/chromeos/contacts/contact_store.h"
#include "chrome/browser/chromeos/contacts/google_contact_store.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

namespace contacts {

namespace {

// Singleton instance.
ContactManager* g_instance = NULL;

}  // namespace

// static
ContactManager* ContactManager::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(g_instance);
  return g_instance;
}

ContactManager::ContactManager()
    : profile_observers_deleter_(&profile_observers_),
      contact_store_factory_(new GoogleContactStoreFactory),
      contact_stores_deleter_(&contact_stores_),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!g_instance);
  g_instance = this;
}

ContactManager::~ContactManager() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(g_instance, this);
  weak_ptr_factory_.InvalidateWeakPtrs();
  g_instance = NULL;
  for (ContactStoreMap::const_iterator it = contact_stores_.begin();
       it != contact_stores_.end(); ++it) {
    it->second->RemoveObserver(this);
  }
}

void ContactManager::SetContactStoreForTesting(
    scoped_ptr<ContactStoreFactory> factory) {
  DCHECK(contact_stores_.empty());
  contact_store_factory_.swap(factory);
}

void ContactManager::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  registrar_.Add(
      this,
      chrome::NOTIFICATION_PROFILE_CREATED,
      content::NotificationService::AllSources());
  registrar_.Add(
      this,
      chrome::NOTIFICATION_PROFILE_DESTROYED,
      content::NotificationService::AllSources());

  // Notify about any already-existing profiles.
  std::vector<Profile*> profiles(
      g_browser_process->profile_manager()->GetLoadedProfiles());
  for (size_t i = 0; i < profiles.size(); ++i)
    HandleProfileCreated(profiles[i]);
}

base::WeakPtr<ContactManagerInterface> ContactManager::GetWeakPtr() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return weak_ptr_factory_.GetWeakPtr();
}

void ContactManager::AddObserver(ContactManagerObserver* observer,
                                 Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(observer);
  DCHECK(profile);
  Observers* observers = GetObserversForProfile(profile, true);
  observers->AddObserver(observer);
}

void ContactManager::RemoveObserver(ContactManagerObserver* observer,
                                    Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(observer);
  DCHECK(profile);
  Observers* observers = GetObserversForProfile(profile, false);
  if (observers)
    observers->RemoveObserver(observer);
}

scoped_ptr<ContactPointers> ContactManager::GetAllContacts(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  scoped_ptr<ContactPointers> contacts(new ContactPointers);
  ContactStoreMap::const_iterator it = contact_stores_.find(profile);
  if (it != contact_stores_.end())
    it->second->AppendContacts(contacts.get());
  return contacts.Pass();
}

const Contact* ContactManager::GetContactById(Profile* profile,
                                              const std::string& contact_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  ContactStoreMap::const_iterator it = contact_stores_.find(profile);
  return it != contact_stores_.end() ?
         it->second->GetContactById(contact_id) :
         NULL;
}

void ContactManager::OnContactsUpdated(ContactStore* store) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  for (ContactStoreMap::const_iterator it = contact_stores_.begin();
       it != contact_stores_.end(); ++it) {
    if (it->second == store) {
      Profile* profile = it->first;
      Observers* observers = GetObserversForProfile(profile, false);
      if (observers) {
        FOR_EACH_OBSERVER(ContactManagerObserver,
                          *observers,
                          OnContactsUpdated(profile));
      }
      return;
    }
  }
  NOTREACHED() << "Got update from unknown contact store " << store;
}

void ContactManager::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED:
      HandleProfileCreated(content::Source<Profile>(source).ptr());
      break;
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      Profile* profile = content::Details<Profile>(details).ptr();
      if (profile)
        HandleProfileDestroyed(profile);
      break;
    }
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
}

ContactManager::Observers* ContactManager::GetObserversForProfile(
    Profile* profile,
    bool create) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ProfileObserversMap::const_iterator it = profile_observers_.find(profile);
  if (it != profile_observers_.end())
    return it->second;
  if (!create)
    return NULL;

  Observers* observers = new Observers;
  profile_observers_[profile] = observers;
  return observers;
}

void ContactManager::HandleProfileCreated(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);

  ContactStoreMap::iterator it = contact_stores_.find(profile);
  if (it != contact_stores_.end())
    return;

  if (!contact_store_factory_->CanCreateContactStoreForProfile(profile))
    return;

  VLOG(1) << "Adding profile " << profile->GetProfileName();
  ContactStore* store = contact_store_factory_->CreateContactStore(profile);
  DCHECK(store);
  store->AddObserver(this);
  store->Init();
  DCHECK_EQ(contact_stores_.count(profile), static_cast<size_t>(0));
  contact_stores_[profile] = store;
}

void ContactManager::HandleProfileDestroyed(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);

  ContactStoreMap::iterator store_it = contact_stores_.find(profile);
  if (store_it != contact_stores_.end()) {
    store_it->second->RemoveObserver(this);
    delete store_it->second;
    contact_stores_.erase(store_it);
  }

  ProfileObserversMap::iterator observer_it = profile_observers_.find(profile);
  if (observer_it != profile_observers_.end()) {
    delete observer_it->second;
    profile_observers_.erase(observer_it);
  }
}

}  // namespace contacts
