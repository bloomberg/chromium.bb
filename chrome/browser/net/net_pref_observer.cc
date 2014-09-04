// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_pref_observer.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_stream_factory.h"

using content::BrowserThread;

NetPrefObserver::NetPrefObserver(PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs);

  base::Closure prefs_callback = base::Bind(&NetPrefObserver::ApplySettings,
                                            base::Unretained(this));
  spdy_disabled_.Init(prefs::kDisableSpdy, prefs, prefs_callback);

  ApplySettings();
}

NetPrefObserver::~NetPrefObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void NetPrefObserver::ApplySettings() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (spdy_disabled_.IsManaged())
    net::HttpStreamFactory::set_spdy_enabled(!*spdy_disabled_);
}

// static
void NetPrefObserver::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kNetworkPredictionEnabled,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDisableSpdy,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}
