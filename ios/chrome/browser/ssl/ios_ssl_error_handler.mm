// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_ssl_error_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/mac/bind_objc_block.h"
#include "base/metrics/histogram_macros.h"
#include "components/captive_portal/captive_portal_detector.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "ios/chrome/browser/ssl/captive_portal_detector_tab_helper.h"
#include "ios/chrome/browser/ssl/captive_portal_features.h"
#include "ios/chrome/browser/ssl/ios_captive_portal_blocking_page.h"
#include "ios/chrome/browser/ssl/ios_ssl_blocking_page.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#include "ios/web/public/browser_state.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

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

  if (!base::FeatureList::IsEnabled(kCaptivePortalFeature)) {
    IOSSSLErrorHandler::RecordCaptivePortalState(web_state);
    IOSSSLErrorHandler::ShowSSLInterstitial(web_state, cert_error, info,
                                            request_url, overridable, callback);
    return;
  }

  // TODO(crbug.com/747405): If certificate error is only a name mismatch,
  // check if the cert is from a known captive portal.

  net::SSLInfo ssl_info(info);
  GURL url(request_url);

  CaptivePortalDetectorTabHelper* tab_helper =
      CaptivePortalDetectorTabHelper::FromWebState(web_state);
  // TODO(crbug.com/754378): The captive portal detection may take a very long
  // time. It should timeout and default to displaying the SSL error page.
  tab_helper->detector()->DetectCaptivePortal(
      GURL(CaptivePortalDetector::kDefaultURL),
      base::BindBlockArc(^(const CaptivePortalDetector::Results& results) {

        IOSSSLErrorHandler::LogCaptivePortalResult(results.result);
        if (results.result == captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL) {
          IOSSSLErrorHandler::ShowCaptivePortalInterstitial(
              web_state, url, results.landing_url, callback);
        } else {
          IOSSSLErrorHandler::ShowSSLInterstitial(
              web_state, cert_error, ssl_info, url, overridable, callback);
        }
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

// static
void IOSSSLErrorHandler::ShowSSLInterstitial(
    web::WebState* web_state,
    int cert_error,
    const net::SSLInfo& info,
    const GURL& request_url,
    bool overridable,
    const base::Callback<void(bool)>& callback) {
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
void IOSSSLErrorHandler::ShowCaptivePortalInterstitial(
    web::WebState* web_state,
    const GURL& request_url,
    const GURL& landing_url,
    const base::Callback<void(bool)>& callback) {
  Tab* tab = LegacyTabHelper::GetTabForWebState(web_state);
  id<IOSCaptivePortalBlockingPageDelegate> delegate =
      tab.iOSCaptivePortalBlockingPageDelegate;
  // IOSCaptivePortalBlockingPage deletes itself when it's dismissed.
  auto dismissal_callback(
      base::Bind(&IOSSSLErrorHandler::InterstitialWasDismissed,
                 base::Unretained(web_state), callback));
  IOSCaptivePortalBlockingPage* page = new IOSCaptivePortalBlockingPage(
      web_state, request_url, landing_url, delegate, dismissal_callback);
  page->Show();
}

// static
void IOSSSLErrorHandler::RecordCaptivePortalState(web::WebState* web_state) {
  CaptivePortalDetectorTabHelper* tab_helper =
      CaptivePortalDetectorTabHelper::FromWebState(web_state);
  tab_helper->detector()->DetectCaptivePortal(
      GURL(CaptivePortalDetector::kDefaultURL),
      base::BindBlockArc(^(const CaptivePortalDetector::Results& results) {
        IOSSSLErrorHandler::LogCaptivePortalResult(results.result);
      }),
      NO_TRAFFIC_ANNOTATION_YET);
}

// static
void IOSSSLErrorHandler::LogCaptivePortalResult(
    captive_portal::CaptivePortalResult result) {
  CaptivePortalStatus status;
  switch (result) {
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
}

// static
void IOSSSLErrorHandler::InterstitialWasDismissed(
    web::WebState* web_state,
    const base::Callback<void(bool)>& callback,
    bool proceed) {
  callback.Run(proceed);
}
