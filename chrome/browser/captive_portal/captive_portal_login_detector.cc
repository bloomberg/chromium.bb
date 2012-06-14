// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/captive_portal/captive_portal_login_detector.h"

#include "chrome/browser/captive_portal/captive_portal_service_factory.h"

namespace captive_portal {

CaptivePortalLoginDetector::CaptivePortalLoginDetector(
    Profile* profile)
    : profile_(profile),
      is_login_tab_(false) {
}

CaptivePortalLoginDetector::~CaptivePortalLoginDetector() {
}

void CaptivePortalLoginDetector::OnStoppedLoading() {
  if (!is_login_tab_)
    return;
  // The service is guaranteed to exist if |is_login_tab_| is true, since it's
  // only set to true once a captive portal is detected.
  CaptivePortalServiceFactory::GetForProfile(profile_)->DetectCaptivePortal();
}

void CaptivePortalLoginDetector::OnCaptivePortalResults(
    Result previous_result,
    Result result) {
  if (result != RESULT_BEHIND_CAPTIVE_PORTAL)
    is_login_tab_ = false;
}

}  // namespace captive_portal
