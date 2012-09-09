// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_pref_observer.h"

#include "chrome/browser/net/predictor.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_stream_factory.h"

using content::BrowserThread;

NetPrefObserver::NetPrefObserver(PrefService* prefs,
                                 prerender::PrerenderManager* prerender_manager,
                                 chrome_browser_net::Predictor* predictor)
    : prerender_manager_(prerender_manager),
      predictor_(predictor) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs);
  DCHECK(predictor);

  network_prediction_enabled_.Init(prefs::kNetworkPredictionEnabled, prefs,
                                   this);
  spdy_disabled_.Init(prefs::kDisableSpdy, prefs, this);

  ApplySettings();
}

NetPrefObserver::~NetPrefObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void NetPrefObserver::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_PREF_CHANGED);
  ApplySettings();
}

void NetPrefObserver::ApplySettings() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  predictor_->EnablePredictor(*network_prediction_enabled_);
  if (prerender_manager_)
    prerender_manager_->set_enabled(*network_prediction_enabled_);
  net::HttpStreamFactory::set_spdy_enabled(!*spdy_disabled_);
}

// static
void NetPrefObserver::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kNetworkPredictionEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDisableSpdy,
                             false,
                             PrefService::UNSYNCABLE_PREF);
}
