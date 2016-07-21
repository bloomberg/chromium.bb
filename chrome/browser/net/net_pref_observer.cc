// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_pref_observer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_stream_factory.h"

using content::BrowserThread;

NetPrefObserver::NetPrefObserver(PrefService* prefs) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(prefs);
}

NetPrefObserver::~NetPrefObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

// static
void NetPrefObserver::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kNetworkPredictionEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kDisableSpdy, false);
}
