// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_pref_observer.h"

#include "base/task.h"
#include "chrome/browser/net/predictor_api.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_type.h"
#include "content/common/notification_details.h"
#include "net/http/http_stream_factory.h"
#include "net/url_request/url_request_throttler_manager.h"

namespace {

// Function (for NewRunnableFunction) to call the set_enforce_throttling
// member on the URLRequestThrottlerManager singleton.
void SetEnforceThrottlingOnThrottlerManager(bool enforce) {
  net::URLRequestThrottlerManager::GetInstance()->set_enforce_throttling(
      enforce);
}

}

NetPrefObserver::NetPrefObserver(PrefService* prefs,
                                 prerender::PrerenderManager* prerender_manager)
    : prerender_manager_(prerender_manager) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(prefs);
  network_prediction_enabled_.Init(prefs::kNetworkPredictionEnabled, prefs,
                                   this);
  spdy_disabled_.Init(prefs::kDisableSpdy, prefs, this);
  http_throttling_enabled_.Init(prefs::kHttpThrottlingEnabled, prefs, this);

  ApplySettings(NULL);
}

NetPrefObserver::~NetPrefObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void NetPrefObserver::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK_EQ(type.value, NotificationType::PREF_CHANGED);

  std::string* pref_name = Details<std::string>(details).ptr();
  ApplySettings(pref_name);
}

void NetPrefObserver::ApplySettings(const std::string* pref_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  chrome_browser_net::EnablePredictor(*network_prediction_enabled_);
  if (prerender_manager_)
    prerender_manager_->set_enabled(*network_prediction_enabled_);
  net::HttpStreamFactory::set_spdy_enabled(!*spdy_disabled_);

  if (!pref_name || *pref_name == prefs::kHttpThrottlingEnabled) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableFunction(SetEnforceThrottlingOnThrottlerManager,
                            *http_throttling_enabled_));
  }
}

// static
void NetPrefObserver::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kNetworkPredictionEnabled, true);
  prefs->RegisterBooleanPref(prefs::kDisableSpdy, false);
  prefs->RegisterBooleanPref(prefs::kHttpThrottlingEnabled, false);
}
