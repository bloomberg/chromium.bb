// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_notifier.h"

#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/policy/configuration_policy_pref_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/notification_service.h"


PrefNotifier::PrefNotifier(PrefService* service, PrefValueStore* value_store)
    : pref_service_(service),
      pref_value_store_(value_store) {
  registrar_.Add(this,
                 NotificationType(NotificationType::POLICY_CHANGED),
                 NotificationService::AllSources());
}

PrefNotifier::~PrefNotifier() {
  DCHECK(CalledOnValidThread());

  // Verify that there are no pref observers when we shut down.
  for (PrefObserverMap::iterator it = pref_observers_.begin();
       it != pref_observers_.end(); ++it) {
    NotificationObserverList::Iterator obs_iterator(*(it->second));
    if (obs_iterator.GetNext()) {
      LOG(WARNING) << "pref observer found at shutdown " << it->first;
    }
  }

  STLDeleteContainerPairSecondPointers(pref_observers_.begin(),
                                       pref_observers_.end());
  pref_observers_.clear();
}

void PrefNotifier::OnPreferenceSet(const char* pref_name,
                                   PrefNotifier::PrefStoreType new_store) {
  if (pref_value_store_->PrefHasChanged(pref_name, new_store))
    FireObservers(pref_name);
}

void PrefNotifier::OnUserPreferenceSet(const char* pref_name) {
  OnPreferenceSet(pref_name, PrefNotifier::USER_STORE);
}

void PrefNotifier::FireObservers(const char* path) {
  DCHECK(CalledOnValidThread());

  // Convert path to a std::string because the Details constructor requires a
  // class.
  std::string path_str(path);
  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path_str);
  if (observer_iterator == pref_observers_.end())
    return;

  NotificationObserverList::Iterator it(*(observer_iterator->second));
  NotificationObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    observer->Observe(NotificationType::PREF_CHANGED,
                      Source<PrefService>(pref_service_),
                      Details<std::string>(&path_str));
  }
}

void PrefNotifier::AddPrefObserver(const char* path,
                                   NotificationObserver* obs) {
  // Get the pref observer list associated with the path.
  NotificationObserverList* observer_list = NULL;
  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    observer_list = new NotificationObserverList;
    pref_observers_[path] = observer_list;
  } else {
    observer_list = observer_iterator->second;
  }

  // Verify that this observer doesn't already exist.
  NotificationObserverList::Iterator it(*observer_list);
  NotificationObserver* existing_obs;
  while ((existing_obs = it.GetNext()) != NULL) {
    DCHECK(existing_obs != obs) << path << " observer already registered";
    if (existing_obs == obs)
      return;
  }

  // Ok, safe to add the pref observer.
  observer_list->AddObserver(obs);
}

void PrefNotifier::RemovePrefObserver(const char* path,
                                     NotificationObserver* obs) {
  DCHECK(CalledOnValidThread());

  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    return;
  }

  NotificationObserverList* observer_list = observer_iterator->second;
  observer_list->RemoveObserver(obs);
}

void PrefNotifier::FireObserversForRefreshedManagedPrefs(
    std::vector<std::string> changed_prefs_paths) {
  DCHECK(CalledOnValidThread());
  std::vector<std::string>::const_iterator current;
  for (current = changed_prefs_paths.begin();
       current != changed_prefs_paths.end();
       ++current) {
    FireObservers(current->c_str());
  }
}

void PrefNotifier::Observe(NotificationType type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  using policy::ConfigurationPolicyPrefStore;

  if (type == NotificationType::POLICY_CHANGED) {
      PrefValueStore::AfterRefreshCallback* callback =
          NewCallback(this,
                      &PrefNotifier::FireObserversForRefreshedManagedPrefs);
      // The notification of the policy refresh can come from any thread,
      // but the manipulation of the PrefValueStore must happen on the UI
      // thread, thus the policy refresh must be explicitly started on it.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(
              pref_value_store_,
              &PrefValueStore::RefreshPolicyPrefs,
              callback));
  }
}
