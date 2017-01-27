// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_ssl_error_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/mac/bind_objc_block.h"
#include "base/metrics/histogram_macros.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"
#include "ios/chrome/browser/ssl/ios_ssl_blocking_page.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/ssl/ssl_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Enum used to record the captive portal detection result.
enum class CaptivePortalStatus {
  UNKNOWN = 0,
  OFFLINE = 1,
  ONLINE = 2,
  PORTAL = 3,
  PROXY_AUTH_REQUIRED = 4,
  COUNT
};

const char kSessionDetectionResultHistogram[] =
    "CaptivePortal.Session.DetectionResult";

using captive_portal::CaptivePortalDetector;

// static
void IOSSSLErrorHandler::HandleSSLError(
    web::WebState* web_state,
    int cert_error,
    const net::SSLInfo& info,
    const GURL& request_url,
    bool overridable,
    const base::Callback<void(bool)>& callback) {
  DCHECK(!web_state->IsShowingWebInterstitial());

  IOSSSLErrorHandler::RecordCaptivePortalState(web_state);

  int options_mask =
      overridable ? security_interstitials::SSLErrorUI::SOFT_OVERRIDE_ENABLED
                  : security_interstitials::SSLErrorUI::STRICT_ENFORCEMENT;
  // SSLBlockingPage deletes itself when it's dismissed.
  auto dismissal_callback(
      base::Bind(&IOSSSLErrorHandler::InterstitialWasDismissed,
                 base::Unretained(web_state), callback));
  IOSSSLBlockingPage* page = new IOSSSLBlockingPage(
      web_state, cert_error, info, request_url, options_mask,
      base::Time::NowFromSystemTime(), dismissal_callback);
  page->Show();
}

// static
void IOSSSLErrorHandler::RecordCaptivePortalState(web::WebState* web_state) {
  CaptivePortalDetectorTabHelper::CreateForWebState(web_state);
  CaptivePortalDetectorTabHelper* tab_helper =
      CaptivePortalDetectorTabHelper::FromWebState(web_state);
  tab_helper->detector()->DetectCaptivePortal(
      GURL(CaptivePortalDetector::kDefaultURL),
      base::BindBlockArc(^(const CaptivePortalDetector::Results& results) {
        CaptivePortalStatus status;
        switch (results.result) {
          case captive_portal::RESULT_INTERNET_CONNECTED:
            status = CaptivePortalStatus::ONLINE;
            break;
          case captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL:
            status = CaptivePortalStatus::PORTAL;
            break;
          default:
            status = CaptivePortalStatus::UNKNOWN;
            break;
        }
        UMA_HISTOGRAM_ENUMERATION(kSessionDetectionResultHistogram,
                                  static_cast<int>(status),
                                  static_cast<int>(CaptivePortalStatus::COUNT));
      }));
}

// static
void IOSSSLErrorHandler::InterstitialWasDismissed(
    web::WebState* web_state,
    const base::Callback<void(bool)>& callback,
    bool proceed) {
  callback.Run(proceed);
}
