// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_set_observer.h"

#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"

PrefSetObserver::PrefSetObserver(PrefService* pref_service,
                                 NotificationObserver* observer)
    : pref_service_(pref_service),
      observer_(observer) {
}

PrefSetObserver::~PrefSetObserver() {
  for (PrefSet::const_iterator i(prefs_.begin()); i != prefs_.end(); ++i)
    pref_service_->RemovePrefObserver(i->c_str(), this);
}

void PrefSetObserver::AddPref(const std::string& pref) {
  if (!prefs_.count(pref) && pref_service_->FindPreference(pref.c_str())) {
    prefs_.insert(pref);
    pref_service_->AddPrefObserver(pref.c_str(), this);
  }
}

void PrefSetObserver::RemovePref(const std::string& pref) {
  if (prefs_.erase(pref))
    pref_service_->RemovePrefObserver(pref.c_str(), this);
}

bool PrefSetObserver::IsObserved(const std::string& pref) {
  return prefs_.count(pref) > 0;
}

bool PrefSetObserver::IsManaged() {
  for (PrefSet::const_iterator i(prefs_.begin()); i != prefs_.end(); ++i) {
    const PrefService::Preference* pref =
        pref_service_->FindPreference(i->c_str());
    if (pref && pref->IsManaged())
      return true;
  }
  return false;
}

// static
PrefSetObserver* PrefSetObserver::CreateProxyPrefSetObserver(
    PrefService* pref_service,
    NotificationObserver* observer) {
  PrefSetObserver* pref_set = new PrefSetObserver(pref_service, observer);
  pref_set->AddPref(prefs::kNoProxyServer);
  pref_set->AddPref(prefs::kProxyAutoDetect);
  pref_set->AddPref(prefs::kProxyServer);
  pref_set->AddPref(prefs::kProxyPacUrl);
  pref_set->AddPref(prefs::kProxyBypassList);

  return pref_set;
}

void PrefSetObserver::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  if (observer_)
    observer_->Observe(type, source, details);
}
