// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SSL_SSL_BLOCKING_PAGE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "chrome/browser/interstitials/security_interstitial_metrics_helper.h"
#include "chrome/browser/interstitials/security_interstitial_page.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

#if defined(ENABLE_EXTENSIONS)
namespace extensions {
class ExperienceSamplingEvent;
}
#endif

class SSLErrorClassification;

// This class is responsible for showing/hiding the interstitial page that is
// shown when a certificate error happens.
// It deletes itself when the interstitial page is closed.
class SSLBlockingPage : public SecurityInterstitialPage {
 public:
  // These represent the commands sent from the interstitial JavaScript. They
  // are defined in chrome/browser/resources/ssl/ssl_errors_common.js.
  // DO NOT reorder or change these without also changing the JavaScript!
  enum SSLBlockingPageCommands {
    CMD_DONT_PROCEED = 0,
    CMD_PROCEED = 1,
    CMD_MORE = 2,
    CMD_RELOAD = 3,
    CMD_HELP = 4,
    CMD_CLOCK = 5
  };

  enum SSLBlockingPageOptionsMask {
    // Indicates whether or not the user could (assuming perfect knowledge)
    // successfully override the error and still get the security guarantees
    // of TLS.
    OVERRIDABLE = 1 << 0,
    // Indicates whether or not the site the user is trying to connect to has
    // requested strict enforcement of certificate validation (e.g. with HTTP
    // Strict-Transport-Security).
    STRICT_ENFORCEMENT = 1 << 1,
    // Indicates whether a user decision had been previously made but the
    // decision has expired.
    EXPIRED_BUT_PREVIOUSLY_ALLOWED = 1 << 2
  };

  // Interstitial type, used in tests.
  static InterstitialPageDelegate::TypeID kTypeForTesting;

  ~SSLBlockingPage() override;

  // Creates an SSL blocking page. If the blocking page isn't shown, the caller
  // is responsible for cleaning up the blocking page, otherwise the
  // interstitial takes ownership when shown. |options_mask| must be a bitwise
  // mask of SSLBlockingPageOptionsMask values.
  SSLBlockingPage(content::WebContents* web_contents,
                  int cert_error,
                  const net::SSLInfo& ssl_info,
                  const GURL& request_url,
                  int options_mask,
                  const base::Time& time_triggered,
                  const base::Callback<void(bool)>& callback);

  // InterstitialPageDelegate method:
  InterstitialPageDelegate::TypeID GetTypeForTesting() const override;

  // Returns true if |options_mask| refers to an overridable SSL error.
  static bool IsOptionsOverridable(int options_mask);

 protected:
  // InterstitialPageDelegate implementation.
  void CommandReceived(const std::string& command) override;
  void OverrideEntry(content::NavigationEntry* entry) override;
  void OverrideRendererPrefs(content::RendererPreferences* prefs) override;
  void OnProceed() override;
  void OnDontProceed() override;

  // SecurityInterstitialPage implementation:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;

 private:
  void NotifyDenyCertificate();
  void NotifyAllowCertificate();

  std::string GetUmaHistogramPrefix() const;
  std::string GetSamplingEventName() const;

  base::Callback<void(bool)> callback_;

  const int cert_error_;
  const net::SSLInfo ssl_info_;
  // There are two ways for the user to override an interstitial:
  //
  // overridable_) By clicking on "Advanced" and then "Proceed".
  //   - This corresponds to "the user can override using the UI".
  // danger_overridable_) By typing the word "danger".
  //   - This is an undocumented workaround.
  //   - This can be set to "false" dynamically to prevent the behaviour.
  const bool overridable_;
  bool danger_overridable_;
  // Has the site requested strict enforcement of certificate errors?
  const bool strict_enforcement_;
  // Did the user previously allow a bad certificate but the decision has now
  // expired?
  const bool expired_but_previously_allowed_;
  scoped_ptr<SSLErrorClassification> ssl_error_classification_;
  scoped_ptr<SecurityInterstitialMetricsHelper> metrics_helper_;
  // The time at which the interstitial was triggered. The interstitial
  // calculates all times relative to this.
  const base::Time time_triggered_;

  // Which type of interstitial this is.
  enum SSLInterstitialReason {
    SSL_REASON_SSL,
    SSL_REASON_BAD_CLOCK
  } interstitial_reason_;

  DISALLOW_COPY_AND_ASSIGN(SSLBlockingPage);
};

#endif  // CHROME_BROWSER_SSL_SSL_BLOCKING_PAGE_H_
