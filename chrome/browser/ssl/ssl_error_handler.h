// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
#define CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_

#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/common_name_mismatch_handler.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "components/ssl_errors/error_classification.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

class CommonNameMismatchHandler;
class Profile;

namespace base {
class Clock;
}

namespace content {
class RenderViewHost;
class WebContents;
}

// This class is responsible for deciding what type of interstitial to show for
// an SSL validation error. The display of the interstitial might be delayed by
// a few seconds (2 by default) while trying to determine the cause of the
// error. During this window, the class will: check for a clock error, wait for
// a name-mismatch suggested URL, or wait for a captive portal result to arrive.
// If there is a name mismatch error and a corresponding suggested URL
// result arrives in this window, the user is redirected to the suggested URL.
// Failing that, if a captive portal detected result arrives in the time window,
// a captive portal error page is shown. If none of these potential error
// causes match, an SSL interstitial is shown.
//
// This class should only be used on the UI thread because its implementation
// uses captive_portal::CaptivePortalService which can only be accessed on the
// UI thread.
class SSLErrorHandler : public content::WebContentsUserData<SSLErrorHandler>,
                        public content::WebContentsObserver,
                        public content::NotificationObserver {
 public:
  typedef base::Callback<void(content::WebContents*)> TimerStartedCallback;

  // Entry point for the class. The parameters are the same as SSLBlockingPage
  // constructor.
  static void HandleSSLError(content::WebContents* web_contents,
                             int cert_error,
                             const net::SSLInfo& ssl_info,
                             const GURL& request_url,
                             int options_mask,
                             std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
                             const base::Callback<void(bool)>& callback);

  // Testing methods.
  static void SetInterstitialDelayForTest(base::TimeDelta delay);
  // The callback pointer must remain valid for the duration of error handling.
  static void SetInterstitialTimerStartedCallbackForTest(
      TimerStartedCallback* callback);
  static void SetClockForTest(base::Clock* testing_clock);

 protected:
  // The parameters are the same as SSLBlockingPage's constructor.
  SSLErrorHandler(content::WebContents* web_contents,
                  int cert_error,
                  const net::SSLInfo& ssl_info,
                  const GURL& request_url,
                  int options_mask,
                  std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
                  const base::Callback<void(bool)>& callback);

  ~SSLErrorHandler() override;

  // Called when an SSL cert error is encountered. Triggers a captive portal
  // check and fires a one shot timer to wait for a "captive portal detected"
  // result to arrive.
  void StartHandlingError();
  const base::OneShotTimer& get_timer() const { return timer_; }

  // These are virtual for tests:
  virtual void CheckForCaptivePortal();
  virtual bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                               GURL* suggested_url) const;
  virtual void CheckSuggestedUrl(const GURL& suggested_url);
  virtual void NavigateToSuggestedURL(const GURL& suggested_url);
  virtual bool IsErrorOverridable() const;
  virtual void ShowCaptivePortalInterstitial(const GURL& landing_url);
  virtual void ShowSSLInterstitial();

  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state);

  // Gets the result of whether the suggested URL is valid. Displays
  // common name mismatch interstitial or ssl interstitial accordingly.
  void CommonNameMismatchHandlerCallback(
      const CommonNameMismatchHandler::SuggestedUrlCheckResult& result,
      const GURL& suggested_url);

 private:
  // content::NotificationObserver:
  void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) override;

  // content::WebContentsObserver:
  void DidStartNavigationToPendingEntry(
      const GURL& url,
      content::NavigationController::ReloadType reload_type) override;

  // content::WebContentsObserver:
  void NavigationStopped() override;

  // Deletes the SSLErrorHandler. This method is called when the page
  // load stops or when there is a new navigation.
  void DeleteSSLErrorHandler();

  content::WebContents* web_contents_;
  const int cert_error_;
  const net::SSLInfo ssl_info_;
  const GURL request_url_;
  const int options_mask_;
  base::Callback<void(bool)> callback_;
  Profile* const profile_;

  content::NotificationRegistrar registrar_;
  base::OneShotTimer timer_;

  std::unique_ptr<CommonNameMismatchHandler> common_name_mismatch_handler_;

  std::unique_ptr<SSLCertReporter> ssl_cert_reporter_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorHandler);
};

#endif  // CHROME_BROWSER_SSL_SSL_ERROR_HANDLER_H_
