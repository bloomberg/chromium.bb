// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_pref_observer.h"

#include "base/bind.h"
#include "base/task.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "net/http/http_stream_factory.h"
#include "net/url_request/url_request_throttler_manager.h"

using content::BrowserThread;

namespace {

// Callback function to call the set_enforce_throttling member on the
// URLRequestThrottlerManager singleton.
void SetEnforceThrottlingOnThrottlerManager(bool enforce) {
  net::URLRequestThrottlerManager::GetInstance()->set_enforce_throttling(
      enforce);
}

}

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
  http_throttling_enabled_.Init(prefs::kHttpThrottlingEnabled, prefs, this);

  ApplySettings(NULL);
}

NetPrefObserver::~NetPrefObserver() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void NetPrefObserver::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_PREF_CHANGED);

  std::string* pref_name = content::Details<std::string>(details).ptr();
  ApplySettings(pref_name);
}

void NetPrefObserver::ApplySettings(const std::string* pref_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  predictor_->EnablePredictor(*network_prediction_enabled_);
  if (prerender_manager_)
    prerender_manager_->set_enabled(*network_prediction_enabled_);
  net::HttpStreamFactory::set_spdy_enabled(!*spdy_disabled_);

  if (!pref_name || *pref_name == prefs::kHttpThrottlingEnabled) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&SetEnforceThrottlingOnThrottlerManager,
                   *http_throttling_enabled_));
  }
}

// static
void NetPrefObserver::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kNetworkPredictionEnabled,
                             true,
                             PrefService::SYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kDisableSpdy,
                             false,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kHttpThrottlingEnabled,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  // TODO(joi): This pref really means "user has not explicitly turned
  // anti-DDoS throttling on or off". Rename it soon (2011/8/26) or
  // remove it altogether (more likely).
  prefs->RegisterBooleanPref(prefs::kHttpThrottlingMayExperiment,
                             true,
                             PrefService::UNSYNCABLE_PREF);

  // For users who created their profile while throttling was off by
  // default, but have never explicitly turned it on or off, we turn
  // it on which is the new default.
  if (prefs->GetBoolean(prefs::kHttpThrottlingMayExperiment)) {
    prefs->SetBoolean(prefs::kHttpThrottlingEnabled, true);
  }
}
