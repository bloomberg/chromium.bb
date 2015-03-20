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
  SSL_ERROR_HANDLER_EVENT_COUNT
};

void RecordUMA(SSLErrorHandlerEvent event) {
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl_error_handler",
                            event,
                            SSL_ERROR_HANDLER_EVENT_COUNT);
}

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
// The delay before displaying the SSL interstitial for cert errors.
// - If a "captive portal detected" result arrives in this many seconds,
//   a captive portal interstitial is displayed.
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
    const base::Callback<void(bool)>& callback) {
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  CaptivePortalTabHelper* captive_portal_tab_helper =
      CaptivePortalTabHelper::FromWebContents(web_contents);
  if (captive_portal_tab_helper) {
    captive_portal_tab_helper->OnSSLCertError(ssl_info);
  }
#endif
  DCHECK(!FromWebContents(web_contents));
  web_contents->SetUserData(UserDataKey(),
                            new SSLErrorHandler(web_contents, cert_error,
                                                ssl_info, request_url,
                                                options_mask, callback));

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
                                 const base::Callback<void(bool)>& callback)
    : content::WebContentsObserver(web_contents),
      web_contents_(web_contents),
      cert_error_(cert_error),
      ssl_info_(ssl_info),
      request_url_(request_url),
      options_mask_(options_mask),
      callback_(callback) {
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  Profile* profile = Profile::FromBrowserContext(
      web_contents->GetBrowserContext());
  registrar_.Add(this,
                 chrome::NOTIFICATION_CAPTIVE_PORTAL_CHECK_RESULT,
                 content::Source<Profile>(profile));
#endif
}

SSLErrorHandler::~SSLErrorHandler() {
}

void SSLErrorHandler::StartHandlingError() {
  RecordUMA(HANDLE_ALL);

#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
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

void SSLErrorHandler::CheckForCaptivePortal() {
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  Profile* profile = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext());
  CaptivePortalService* captive_portal_service =
      CaptivePortalServiceFactory::GetForProfile(profile);
  captive_portal_service->DetectCaptivePortal();
#else
  NOTREACHED();
#endif
}

void SSLErrorHandler::ShowCaptivePortalInterstitial(const GURL& landing_url) {
#if defined(ENABLE_CAPTIVE_PORTAL_DETECTION)
  // Show captive portal blocking page. The interstitial owns the blocking page.
  RecordUMA(SSLBlockingPage::IsOptionsOverridable(options_mask_) ?
            SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE :
            SHOW_CAPTIVE_PORTAL_INTERSTITIAL_NONOVERRIDABLE);
  (new CaptivePortalBlockingPage(web_contents_, request_url_, landing_url,
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
  RecordUMA(SSLBlockingPage::IsOptionsOverridable(options_mask_) ?
            SHOW_SSL_INTERSTITIAL_OVERRIDABLE :
            SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE);
  (new SSLBlockingPage(web_contents_, cert_error_, ssl_info_, request_url_,
                       options_mask_, base::Time::NowFromSystemTime(),
                       callback_))->Show();
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

// Destroy the error handler on all new navigations. This ensures that the
// handler is properly recreated when a hanging page is navigated to an SSL
// error, even when the tab's WebContents doesn't change.
void SSLErrorHandler::DidStartNavigationToPendingEntry(
    const GURL& url,
    content::NavigationController::ReloadType reload_type) {
  // Need to explicity deny the certificate via the callback, otherwise memory
  // is leaked.
  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(false);
  }
  web_contents_->RemoveUserData(UserDataKey());
}
