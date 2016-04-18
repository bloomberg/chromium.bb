// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_BAD_CLOCK_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SSL_BAD_CLOCK_BLOCKING_PAGE_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/interstitials/security_interstitial_page.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "components/ssl_errors/error_classification.h"
#include "net/ssl/ssl_info.h"

class CertReportHelper;
class GURL;

namespace security_interstitials {
class BadClockUI;
}

// This class is responsible for showing/hiding the interstitial page that
// occurs when an SSL error is triggered by a clock misconfiguration. It
// creates the UI using security_interstitials::BadClockUI and then
// displays it. It deletes itself when the interstitial page is closed.
class BadClockBlockingPage : public SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static InterstitialPageDelegate::TypeID kTypeForTesting;

  // If the blocking page isn't shown, the caller is responsible for cleaning
  // up the blocking page. Otherwise, the interstitial takes ownership when
  // shown.
  BadClockBlockingPage(content::WebContents* web_contents,
                       int cert_error,
                       const net::SSLInfo& ssl_info,
                       const GURL& request_url,
                       const base::Time& time_triggered,
                       ssl_errors::ClockState clock_state,
                       scoped_ptr<SSLCertReporter> ssl_cert_reporter,
                       const base::Callback<void(bool)>& callback);

  ~BadClockBlockingPage() override;

  // InterstitialPageDelegate method:
  InterstitialPageDelegate::TypeID GetTypeForTesting() const override;

  void SetSSLCertReporterForTesting(
      scoped_ptr<SSLCertReporter> ssl_cert_reporter);

 protected:
  // InterstitialPageDelegate implementation:
  void CommandReceived(const std::string& command) override;
  void OverrideEntry(content::NavigationEntry* entry) override;
  void OverrideRendererPrefs(content::RendererPreferences* prefs) override;
  void OnDontProceed() override;

  // SecurityInterstitialPage implementation:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;
  void AfterShow() override;

 private:
  void NotifyDenyCertificate();

  base::Callback<void(bool)> callback_;
  const net::SSLInfo ssl_info_;
  const base::Time time_triggered_;
  scoped_ptr<ChromeControllerClient> controller_;

  scoped_ptr<security_interstitials::BadClockUI> bad_clock_ui_;
  scoped_ptr<CertReportHelper> cert_report_helper_;

  DISALLOW_COPY_AND_ASSIGN(BadClockBlockingPage);
};

#endif  // CHROME_BROWSER_SSL_BAD_CLOCK_BLOCKING_PAGE_H_
