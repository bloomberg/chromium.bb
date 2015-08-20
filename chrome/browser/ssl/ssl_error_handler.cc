// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_error_handler.h"

#include "base/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_blocking_page.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "chrome/browser/ssl/ssl_error_classification.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
#include "chrome/browser/captive_portal/captive_portal_service.h"
#include "chrome/browser/captive_portal/captive_portal_service_factory.h"
#include "chrome/browser/captive_portal/captive_portal_tab_helper.h"
#include "chrome/browser/ssl/captive_portal_blocking_page.h"
#endif

namespace {

// The type of the delay before displaying the SSL interstitial. This can be
// changed in tests.
SSLErrorHandler::InterstitialDelayType g_interstitial_delay_type =
    SSLErrorHandler::NORMAL;

// Callback to call when the interstitial timer is started. Used for testing.
SSLErrorHandler::TimerStartedCallback* g_timer_started_callback = nullptr;

// Events for UMA.
enum SSLErrorHandlerEvent {
  HANDLE_ALL,
  SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE,
  SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE,
  SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE,
  SHOW_SSL_INTERSTITIAL_OVERRIDABLE,
  WWW_MISMATCH_FOUND,
  WWW_MISMATCH_URL_AVAILABLE,
  WWW_MISMATCH_URL_NOT_AVAILABLE,
  SSL_ERROR_HANDLER_EVENT_COUNT
};

void RecordUMA(SSLErrorHandlerEvent event) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl_error_handler",
                            event,
                            SSL_ERROR_HANDLER_EVENT_COUNT);
}

// The delay before displaying the SSL interstitial for cert errors.
// - If a "captive portal detected" or "suggested URL valid" result
//   arrives in this many seconds, then a captive portal interstitial
//   or a common name mismatch interstitial is displayed.
// - Otherwise, an SSL interstitial is displayed.
const int kDefaultInterstitialDisplayDelayInSeconds = 2;

base::TimeDelta GetInterstitialDisplayDelay(
    SSLErrorHandler::InterstitialDelayType delay) {
  switch (delay) {
    case SSLErrorHandler::LONG:
      return base::TimeDelta::FromHours(1);

    case SSLErrorHandler::NONE:
      return base::TimeDelta();

    case SSLErrorHandler::NORMAL:
      return base::TimeDelta::FromSeconds(
          kDefaultInterstitialDisplayDelayInSeconds);

    default:
      NOTREACHED();
  }
  return base::TimeDelta();
}

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
bool IsCaptivePortalInterstitialEnabled() {
  return base::FieldTrialList::FindFullName("CaptivePortalInterstitial") ==
         "Enabled";
}
#endif

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SSLErrorHandler);

void SSLErrorHandler::HandleSSLError(
    content::WebContents* web_contents,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    int options_mask,
    scoped_ptr<SSLCertReporter> ssl_cert_reporter,
    const base::Callback<void(bool)>& callback) {
  DCHECK(!FromWebContents(web_contents));
  web_contents->SetUserData(
      UserDataKey(),
      new SSLErrorHandler(web_contents, cert_error, ssl_info, request_url,
                          options_mask, ssl_cert_reporter.Pass(), callback));

  SSLErrorHandler* error_handler =
      SSLErrorHandler::FromWebContents(web_contents);
  error_handler->StartHandlingError();
}

// static
void SSLErrorHandler::SetInterstitialDelayTypeForTest(
    SSLErrorHandler::InterstitialDelayType delay) {
  g_interstitial_delay_type = delay;
}

// static
void SSLErrorHandler::SetInterstitialTimerStartedCallbackForTest(
    TimerStartedCallback* callback) {
  DCHECK(!callback || !callback->is_null());
  g_timer_started_callback = callback;
}

SSLErrorHandler::SSLErrorHandler(content::WebContents* web_contents,
                                 int cert_error,
                                 const net::SSLInfo& ssl_info,
                                 const GURL& request_url,
                                 int options_mask,
                                 scoped_ptr<SSLCertReporter> ssl_cert_reporter,
                                 const base::Callback<void(bool)>& callback)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      request_url_(request_url),
      options_mask_(options_mask),
      callback_(callback),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      ssl_cert_reporter_(ssl_cert_reporter.Pass()) {}

SSLErrorHandler::~SSLErrorHandler() {
}

void SSLErrorHandler::StartHandlingError() {
  RecordUMA(HANDLE_ALL);

  std::vector<std::string> dns_names;
  ssl_info_.cert->GetDNSNames(&dns_names);
  DCHECK(!dns_names.empty());
  GURL suggested_url;
  if (ssl_info_.cert_status == net::CERT_STATUS_COMMON_NAME_INVALID &&
      GetSuggestedUrl(dns_names, &suggested_url)) {
    RecordUMA(WWW_MISMATCH_FOUND);
    net::CertStatus extra_cert_errors =
        ssl_info_.cert_status ^ net::CERT_STATUS_COMMON_NAME_INVALID;

    // Show the SSL intersitial if |CERT_STATUS_COMMON_NAME_INVALID| is not
    // the only error. Need not check for captive portal in this case.
    // (See the comment below).
    if (net::IsCertStatusError(extra_cert_errors) &&
        !net::IsCertStatusMinorError(ssl_info_.cert_status)) {
      ShowSSLInterstitial();
      return;
    }
    CheckSuggestedUrl(suggested_url);
    timer_.Start(FROM_HERE,
                 GetInterstitialDisplayDelay(g_interstitial_delay_type), this,
                 &SSLErrorHandler::OnTimerExpired);
    if (g_timer_started_callback)
      g_timer_started_callback->Run(web_contents_);

    // Do not check for a captive portal in this case, because a captive
    // portal most likely cannot serve a valid certificate which passes the
    // similarity check.
    return;
  }

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalTabHelper* captive_portal_tab_helper =
      CaptivePortalTabHelper::FromWebContents(web_contents_);
  if (captive_portal_tab_helper) {
    captive_portal_tab_helper->OnSSLCertError(ssl_info_);
  }

  registrar_.Add(this, chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                 content::Source<Profile>(profile_));

  if (IsCaptivePortalInterstitialEnabled()) {
    CheckForCaptivePortal();
    timer_.Start(FROM_HERE,
                 GetInterstitialDisplayDelay(g_interstitial_delay_type),
                 this, &SSLErrorHandler::OnTimerExpired);
    if (g_timer_started_callback)
      g_timer_started_callback->Run(web_contents_);
    return;
  }
#endif
  // Display an SSL interstitial.
  ShowSSLInterstitial();
}

void SSLErrorHandler::OnTimerExpired() {
  ShowSSLInterstitial();
}

bool SSLErrorHandler::GetSuggestedUrl(const std::vector<std::string>& dns_names,
                                      GURL* suggested_url) const {
  return CommonNameMismatchHandler::GetSuggestedUrl(request_url_, dns_names,
                                                    suggested_url);
}

void SSLErrorHandler::CheckSuggestedUrl(const GURL& suggested_url) {
  scoped_refptr<net::URLRequestContextGetter> request_context(
      profile_->GetRequestContext());
  common_name_mismatch_handler_.reset(
      new CommonNameMismatchHandler(request_url_, request_context));

  common_name_mismatch_handler_->CheckSuggestedUrl(
      suggested_url,
      base::Bind(&SSLErrorHandler::CommonNameMismatchHandlerCallback,
                 base::Unretained(this)));
}

void SSLErrorHandler::NavigateToSuggestedURL(const GURL& suggested_url) {
  content::NavigationController::LoadURLParams load_params(suggested_url);
  load_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  web_contents()->GetController().LoadURLWithParams(load_params);
}

void SSLErrorHandler::CheckForCaptivePortal() {
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(profile_);
  captive_portal_service->DetectCaptivePortal();
#else
  NOTREACHED();
#endif
}

void SSLErrorHandler::ShowCaptivePortalInterstitial(const GURL& landing_url) {
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Show captive portal blocking page. The interstitial owns the blocking page.
  RecordUMA(SSLBlockingPage::IsOverridable(options_mask_, profile_)
                ? SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE
                : SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE);
  (new CaptivePortalBlockingPage(web_contents_, request_url_, landing_url,
                                 ssl_cert_reporter_.Pass(), ssl_info_,
                                 callback_))->Show();
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents_->RemoveUserData(UserDataKey());
#else
  NOTREACHED();
#endif
}

void SSLErrorHandler::ShowSSLInterstitial() {
  // Show SSL blocking page. The interstitial owns the blocking page.
  RecordUMA(SSLBlockingPage::IsOverridable(options_mask_, profile_)
                ? SHOW_SSL_INTERSTITIAL_OVERRIDABLE
                : SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE);

  (new SSLBlockingPage(web_contents_, cert_error_, ssl_info_, request_url_,
                       options_mask_, base::Time::NowFromSystemTime(),
                       ssl_cert_reporter_.Pass(), callback_))
      ->Show();
  // Once an interstitial is displayed, no need to keep the handler around.
  // This is the equivalent of "delete this".
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  if (type == chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT) {
    timer_.Stop();
    CaptivePortalService::Results* results =
        content::Details<CaptivePortalService::Results>(details).ptr();
    if (results->result == captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL)
      ShowCaptivePortalInterstitial(results->landing_url);
    else
      ShowSSLInterstitial();
  }
#endif
}

void SSLErrorHandler::DidStartNavigationToPendingEntry(
    const GURL& /* url */,
    content::NavigationController::ReloadType /* reload_type */) {
// Destroy the error handler on all new navigations. This ensures that the
// handler is properly recreated when a hanging page is navigated to an SSL
// error, even when the tab's WebContents doesn't change.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::NavigationStopped() {
// Destroy the error handler when the page load is stopped.
  DeleteSSLErrorHandler();
}

void SSLErrorHandler::DeleteSSLErrorHandler() {
  // Need to explicity deny the certificate via the callback, otherwise memory
  // is leaked.
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(false);
  }
  if (common_name_mismatch_handler_) {
    common_name_mismatch_handler_->Cancel();
    common_name_mismatch_handler_.reset();
  }
  web_contents_->RemoveUserData(UserDataKey());
}

void SSLErrorHandler::CommonNameMismatchHandlerCallback(
    const CommonNameMismatchHandler::SuggestedUrlCheckResult& result,
    const GURL& suggested_url) {
  timer_.Stop();
  if (result == CommonNameMismatchHandler::SuggestedUrlCheckResult::
                    SUGGESTED_URL_AVAILABLE) {
    RecordUMA(WWW_MISMATCH_URL_AVAILABLE);
    NavigateToSuggestedURL(suggested_url);
  } else {
    RecordUMA(WWW_MISMATCH_URL_NOT_AVAILABLE);
    ShowSSLInterstitial();
  }
}
