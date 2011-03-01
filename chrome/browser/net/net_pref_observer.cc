// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_pref_observer.h"

#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "net/http/http_stream_factory.h"

NetPrefObserver::NetPrefObserver(PrefService* prefs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  dns_prefetching_enabled_.Init(prefs::kDnsPrefetchingEnabled, prefs, this);
  spdy_disabled_.Init(prefs::kDisableSpdy, prefs, this);

  ApplySettings();
}

NetPrefObserver::~NetPrefObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void NetPrefObserver::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  ApplySettings();
}

void NetPrefObserver::ApplySettings() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chrome_browser_net::EnablePredictor(*dns_prefetching_enabled_);
  net::HttpStreamFactory::set_spdy_enabled(!*spdy_disabled_);
}

// static
void NetPrefObserver::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kDisableSpdy, false);
}
