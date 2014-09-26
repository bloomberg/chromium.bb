// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/service/cast_service_android.h"

#include "chromecast/android/chromecast_config_android.h"

namespace chromecast {

// static
CastService* CastService::Create(
    content::BrowserContext* browser_context,
    net::URLRequestContextGetter* request_context_getter) {
  return new CastServiceAndroid(browser_context);
}

CastServiceAndroid::CastServiceAndroid(content::BrowserContext* browser_context)
    : CastService(browser_context) {
}

CastServiceAndroid::~CastServiceAndroid() {
}

void CastServiceAndroid::Initialize() {
  // TODO(gunsch): Wire this the SendUsageStatsChanged callback once
  // CastService::Delegate is added.
}

void CastServiceAndroid::StartInternal() {
}

void CastServiceAndroid::StopInternal() {
}

}  // namespace chromecast
