// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"

#include "base/memory/ptr_util.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_state/web_state.h"

DEFINE_WEB_STATE_USER_DATA_KEY(CaptivePortalDetectorTabHelper);

captive_portal::CaptivePortalDetector*
CaptivePortalDetectorTabHelper::detector() {
  return detector_.get();
}

CaptivePortalDetectorTabHelper::CaptivePortalDetectorTabHelper(
    web::WebState* web_state)
    : detector_(base::MakeUnique<captive_portal::CaptivePortalDetector>(
          web_state->GetBrowserState()->GetRequestContext())) {}

CaptivePortalDetectorTabHelper::~CaptivePortalDetectorTabHelper() = default;
