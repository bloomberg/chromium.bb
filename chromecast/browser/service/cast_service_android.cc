// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service/cast_service_android.h"

#include "chromecast/android/chromecast_config_android.h"

namespace chromecast {

// static
scoped_ptr<CastService> CastService::Create(
    content::BrowserContext* browser_context,
    PrefService* pref_service,
    net::URLRequestContextGetter* request_context_getter,
    const OptInStatsChangedCallback& opt_in_stats_callback) {
  return scoped_ptr<CastService>(new CastServiceAndroid(browser_context,
                                                        pref_service,
                                                        opt_in_stats_callback));
}

CastServiceAndroid::CastServiceAndroid(
    content::BrowserContext* browser_context,
    PrefService* pref_service,
    const OptInStatsChangedCallback& opt_in_stats_callback)
    : CastService(browser_context, pref_service, opt_in_stats_callback) {
}

CastServiceAndroid::~CastServiceAndroid() {
}

void CastServiceAndroid::Initialize() {
  android::ChromecastConfigAndroid::GetInstance()->
      SetSendUsageStatsChangedCallback(opt_in_stats_callback());
}

void CastServiceAndroid::StartInternal() {
}

void CastServiceAndroid::StopInternal() {
}

}  // namespace chromecast
