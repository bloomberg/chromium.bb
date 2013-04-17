// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/eula_accepted_notifier_mobile.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"

EulaAcceptedNotifierMobile::EulaAcceptedNotifierMobile(
    PrefService* local_state)
    : local_state_(local_state) {
}

EulaAcceptedNotifierMobile::~EulaAcceptedNotifierMobile() {
}

bool EulaAcceptedNotifierMobile::IsEulaAccepted() {
  if (local_state_->GetBoolean(prefs::kEulaAccepted))
    return true;

  // Register for the notification, if this is the first time.
  if (registrar_.IsEmpty()) {
    registrar_.Init(local_state_);
    registrar_.Add(prefs::kEulaAccepted,
                   base::Bind(&EulaAcceptedNotifierMobile::OnPrefChanged,
                              base::Unretained(this)));
  }
  return false;
}

void EulaAcceptedNotifierMobile::OnPrefChanged() {
  DCHECK(!registrar_.IsEmpty());
  registrar_.RemoveAll();

  DCHECK(local_state_->GetBoolean(prefs::kEulaAccepted));
  NotifyObserver();
}

// static
EulaAcceptedNotifier* EulaAcceptedNotifier::Create() {
  PrefService* local_state = g_browser_process->local_state();
  // If the |kEulaAccepted| pref is not registered, return NULL which is
  // equivalent to not needing to check the EULA. This is the case for some
  // tests for higher-level classes that use the EulaAcceptNotifier.
  if (local_state->FindPreference(prefs::kEulaAccepted) == NULL)
    return NULL;
  return new EulaAcceptedNotifierMobile(local_state);
}
