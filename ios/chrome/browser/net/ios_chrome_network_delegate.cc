// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/net/ios_chrome_network_delegate.h"

#include <stdlib.h>

#include "base/base_paths.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/debug/stack_trace.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/path_service.h"
#include "base/profiler/scoped_tracker.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/web/public/web_thread.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_options.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request.h"

namespace {

const char kDNTHeader[] = "DNT";

void ReportInvalidReferrerSendOnUI() {
  base::RecordAction(
      base::UserMetricsAction("Net.URLRequest_StartJob_InvalidReferrer"));
}

void ReportInvalidReferrerSend(const GURL& target_url,
                               const GURL& referrer_url) {
  LOG(ERROR) << "Cancelling request to " << target_url
             << " with invalid referrer " << referrer_url;
  // Record information to help debug http://crbug.com/422871
  if (!target_url.SchemeIsHTTPOrHTTPS())
    return;
  web::WebThread::PostTask(web::WebThread::UI, FROM_HERE,
                           base::Bind(&ReportInvalidReferrerSendOnUI));
  base::debug::DumpWithoutCrashing();
  NOTREACHED();
}

// Record network errors that HTTP requests complete with, including OK and
// ABORTED.
void RecordNetworkErrorHistograms(const net::URLRequest* request,
                                  int net_error) {
  if (request->url().SchemeIs("http")) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("Net.HttpRequestCompletionErrorCodes",
                                std::abs(net_error));

    if (request->load_flags() & net::LOAD_MAIN_FRAME_DEPRECATED) {
      UMA_HISTOGRAM_SPARSE_SLOWLY(
          "Net.HttpRequestCompletionErrorCodes.MainFrame", std::abs(net_error));
    }
  }
}

}  // namespace

IOSChromeNetworkDelegate::IOSChromeNetworkDelegate()
    : enable_do_not_track_(nullptr) {}

IOSChromeNetworkDelegate::~IOSChromeNetworkDelegate() {}

// static
void IOSChromeNetworkDelegate::InitializePrefsOnUIThread(
    BooleanPrefMember* enable_do_not_track,
    PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  if (enable_do_not_track) {
    enable_do_not_track->Init(prefs::kEnableDoNotTrack, pref_service);
    enable_do_not_track->MoveToThread(
        web::WebThread::GetTaskRunnerForThread(web::WebThread::IO));
  }
}

int IOSChromeNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456327 URLRequest::IOSChromeNetworkDelegate::OnBeforeURLRequest"));

  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile2(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456327 URLRequest::IOSChromeNetworkDelegate::OnBeforeURLRequest 2"));

  if (enable_do_not_track_ && enable_do_not_track_->GetValue())
    request->SetExtraRequestHeaderByName(kDNTHeader, "1", true /* override */);

  // TODO(mmenke): Remove ScopedTracker below once crbug.com/456327 is fixed.
  tracked_objects::ScopedTracker tracking_profile4(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "456327 URLRequest::IOSChromeNetworkDelegate::OnBeforeURLRequest 4"));

  return net::OK;
}

void IOSChromeNetworkDelegate::OnCompleted(net::URLRequest* request,
                                           bool started,
                                           int net_error) {
  RecordNetworkErrorHistograms(request, net_error);
}

net::NetworkDelegate::AuthRequiredResponse
IOSChromeNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  return net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool IOSChromeNetworkDelegate::OnCanGetCookies(
    const net::URLRequest& request,
    const net::CookieList& cookie_list) {
  // Null during tests, or when we're running in the system context.
  if (!cookie_settings_)
    return true;

  return cookie_settings_->IsCookieAccessAllowed(
      request.url(), request.first_party_for_cookies());
}

bool IOSChromeNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                              const std::string& cookie_line,
                                              net::CookieOptions* options) {
  // Null during tests, or when we're running in the system context.
  if (!cookie_settings_)
    return true;

  return cookie_settings_->IsCookieAccessAllowed(
      request.url(), request.first_party_for_cookies());
}

bool IOSChromeNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& path) const {
  return true;
}

bool IOSChromeNetworkDelegate::OnCanEnablePrivacyMode(
    const GURL& url,
    const GURL& first_party_for_cookies) const {
  // Null during tests, or when we're running in the system context.
  if (!cookie_settings_.get())
    return false;

  return !cookie_settings_->IsCookieAccessAllowed(url, first_party_for_cookies);
}

bool IOSChromeNetworkDelegate::
    OnCancelURLRequestWithPolicyViolatingReferrerHeader(
        const net::URLRequest& request,
        const GURL& target_url,
        const GURL& referrer_url) const {
  ReportInvalidReferrerSend(target_url, referrer_url);
  return true;
}
