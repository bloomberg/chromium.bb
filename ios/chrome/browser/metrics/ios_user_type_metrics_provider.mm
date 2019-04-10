// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_user_type_metrics_provider.h"

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/user/special_user_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSUserTypeMetricsProvider::IOSUserTypeMetricsProvider() {}

IOSUserTypeMetricsProvider::~IOSUserTypeMetricsProvider() {}

void IOSUserTypeMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  ios::GetChromeBrowserProvider()
      ->GetSpecialUserProvider()
      ->RecordUserTypeMetrics();
}
