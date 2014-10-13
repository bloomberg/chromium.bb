// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service/cast_service_android.h"

#include "chromecast/android/chromecast_config_android.h"

namespace chromecast {

// static
CastService* CastService::Create(
    content::BrowserContext* browser_context,
    net::URLRequestContextGetter* request_context_getter,
    const OptInStatsChangedCallback& opt_in_stats_callback) {
  return new CastServiceAndroid(browser_context, opt_in_stats_callback);
}

CastServiceAndroid::CastServiceAndroid(
    content::BrowserContext* browser_context,
    const OptInStatsChangedCallback& opt_in_stats_callback)
    : CastService(browser_context, opt_in_stats_callback) {
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
