// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/service/cast_service_android.h"

#include "base/bind.h"
#include "chromecast/android/chromecast_config_android.h"
#include "chromecast/browser/metrics/cast_metrics_service_client.h"

namespace chromecast {

// static
scoped_ptr<CastService> CastService::Create(
    content::BrowserContext* browser_context,
    PrefService* pref_service,
    metrics::CastMetricsServiceClient* metrics_service_client,
    net::URLRequestContextGetter* request_context_getter) {
  return scoped_ptr<CastService>(
      new CastServiceAndroid(browser_context,
                             pref_service,
                             metrics_service_client));
}

CastServiceAndroid::CastServiceAndroid(
    content::BrowserContext* browser_context,
    PrefService* pref_service,
    metrics::CastMetricsServiceClient* metrics_service_client)
    : CastService(browser_context, pref_service, metrics_service_client) {
}

CastServiceAndroid::~CastServiceAndroid() {
}

void CastServiceAndroid::InitializeInternal() {
  android::ChromecastConfigAndroid::GetInstance()->
      SetSendUsageStatsChangedCallback(
          base::Bind(&metrics::CastMetricsServiceClient::EnableMetricsService,
                     base::Unretained(metrics_service_client())));
}

void CastServiceAndroid::FinalizeInternal() {
  android::ChromecastConfigAndroid::GetInstance()->
      SetSendUsageStatsChangedCallback(base::Callback<void(bool)>());
}

void CastServiceAndroid::StartInternal() {
}

void CastServiceAndroid::StopInternal() {
}

}  // namespace chromecast
