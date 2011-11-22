// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_notifier_impl.h"

#include "base/stl_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"

PrefNotifierImpl::PrefNotifierImpl()
    : pref_service_(NULL) {
}

PrefNotifierImpl::PrefNotifierImpl(PrefService* service)
    : pref_service_(service) {
}

PrefNotifierImpl::~PrefNotifierImpl() {
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

void PrefNotifierImpl::AddPrefObserver(const char* path,
                                       content::NotificationObserver* obs) {
  // Get the pref observer list associated with the path.
  NotificationObserverList* observer_list = NULL;
  const PrefObserverMap::iterator observer_iterator =
      pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    observer_list = new NotificationObserverList;
    pref_observers_[path] = observer_list;
  } else {
    observer_list = observer_iterator->second;
  }

  // Verify that this observer doesn't already exist.
  NotificationObserverList::Iterator it(*observer_list);
  content::NotificationObserver* existing_obs;
  while ((existing_obs = it.GetNext()) != NULL) {
    DCHECK(existing_obs != obs) << path << " observer already registered";
    if (existing_obs == obs)
      return;
  }

  // Ok, safe to add the pref observer.
  observer_list->AddObserver(obs);
}

void PrefNotifierImpl::RemovePrefObserver(const char* path,
                                          content::NotificationObserver* obs) {
  DCHECK(CalledOnValidThread());

  const PrefObserverMap::iterator observer_iterator =
      pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    return;
  }

  NotificationObserverList* observer_list = observer_iterator->second;
  observer_list->RemoveObserver(obs);
}

void PrefNotifierImpl::OnPreferenceChanged(const std::string& path) {
  FireObservers(path);
}

void PrefNotifierImpl::OnInitializationCompleted(bool succeeded) {
  DCHECK(CalledOnValidThread());

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PREF_INITIALIZATION_COMPLETED,
      content::Source<PrefService>(pref_service_),
      content::Details<bool>(&succeeded));
}

void PrefNotifierImpl::FireObservers(const std::string& path) {
  DCHECK(CalledOnValidThread());

  // Only send notifications for registered preferences.
  if (!pref_service_->FindPreference(path.c_str()))
    return;

  const PrefObserverMap::iterator observer_iterator =
      pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end())
    return;

  NotificationObserverList::Iterator it(*(observer_iterator->second));
  content::NotificationObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    observer->Observe(chrome::NOTIFICATION_PREF_CHANGED,
                      content::Source<PrefService>(pref_service_),
                      content::Details<const std::string>(&path));
  }
}

void PrefNotifierImpl::SetPrefService(PrefService* pref_service) {
  DCHECK(pref_service_ == NULL);
  pref_service_ = pref_service;
}
