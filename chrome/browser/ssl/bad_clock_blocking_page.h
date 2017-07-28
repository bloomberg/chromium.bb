// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_BAD_CLOCK_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SSL_BAD_CLOCK_BLOCKING_PAGE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/ssl/ssl_cert_reporter.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/ssl_errors/error_classification.h"
#include "content/public/browser/certificate_request_result_type.h"
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
class BadClockBlockingPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  // Interstitial type, used in tests.
  static const InterstitialPageDelegate::TypeID kTypeForTesting;

  // If the blocking page isn't shown, the caller is responsible for cleaning
  // up the blocking page. Otherwise, the interstitial takes ownership when
  // shown.
  BadClockBlockingPage(content::WebContents* web_contents,
                       int cert_error,
                       const net::SSLInfo& ssl_info,
                       const GURL& request_url,
                       const base::Time& time_triggered,
                       ssl_errors::ClockState clock_state,
                       std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
                       const base::Callback<void(
                           content::CertificateRequestResultType)>& callback);

  ~BadClockBlockingPage() override;

  // InterstitialPageDelegate method:
  InterstitialPageDelegate::TypeID GetTypeForTesting() const override;

  void SetSSLCertReporterForTesting(
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter);

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

 private:
  void NotifyDenyCertificate();

  base::Callback<void(content::CertificateRequestResultType)> callback_;
  const net::SSLInfo ssl_info_;

  const std::unique_ptr<CertReportHelper> cert_report_helper_;
  const std::unique_ptr<security_interstitials::BadClockUI> bad_clock_ui_;

  DISALLOW_COPY_AND_ASSIGN(BadClockBlockingPage);
};

#endif  // CHROME_BROWSER_SSL_BAD_CLOCK_BLOCKING_PAGE_H_
