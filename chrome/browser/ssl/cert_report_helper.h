// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_CERT_REPORT_HELPER_H_
#define CHROME_BROWSER_SSL_CERT_REPORT_HELPER_H_

#include <string>

#include "base/macros.h"
#include "components/certificate_reporting/error_report.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace content {
class WebContents;
}

namespace security_interstitials {
class MetricsHelper;
}

class SSLCertReporter;

// CertReportHelper helps SSL interstitials report invalid certificate
// chains.
class CertReportHelper {
 public:
  // Constants for the HTTPSErrorReporter Finch experiment
  static const char kFinchExperimentName[];
  static const char kFinchGroupShowPossiblySend[];
  static const char kFinchGroupDontShowDontSend[];
  static const char kFinchParamName[];

  CertReportHelper(std::unique_ptr<SSLCertReporter> ssl_cert_reporter,
                   content::WebContents* web_contents,
                   const GURL& request_url,
                   const net::SSLInfo& ssl_info,
                   certificate_reporting::ErrorReport::InterstitialReason
                       interstitial_reason,
                   bool overridable,
                   const base::Time& interstitial_time,
                   security_interstitials::MetricsHelper* metrics_helper);

  virtual ~CertReportHelper();

  // Populates data that JavaScript code on the interstitial uses to show
  // the checkbox.
  void PopulateExtendedReportingOption(base::DictionaryValue* load_time_data);

  // Sends a report about an invalid certificate to the
  // server. |user_proceeded| indicates whether the user clicked through
  // the interstitial or not, and will be included in the report.
  void FinishCertCollection(
      certificate_reporting::ErrorReport::ProceedDecision user_proceeded);

  // Allows tests to inject a mock reporter.
  void SetSSLCertReporterForTesting(
      std::unique_ptr<SSLCertReporter> ssl_cert_reporter);

 private:
  // Checks whether a checkbox should be shown on the page that allows
  // the user to opt in to Safe Browsing extended reporting.
  bool ShouldShowCertificateReporterCheckbox();

  // Returns true if a certificate report should be sent for the SSL
  // error for this page.
  bool ShouldReportCertificateError();

  // Returns the boolean value of the given |pref| from the PrefService of the
  // Profile associated with |web_contents_|.
  bool IsPrefEnabled(const char* pref);

  // Handles reports of invalid SSL certificates.
  std::unique_ptr<SSLCertReporter> ssl_cert_reporter_;
  // The WebContents for which this helper sends reports.
  content::WebContents* web_contents_;
  // The URL for which this helper sends reports.
  const GURL request_url_;
  // The SSLInfo used in this helper's report.
  const net::SSLInfo ssl_info_;
  // The reason for the interstitial, included in this helper's report.
  certificate_reporting::ErrorReport::InterstitialReason interstitial_reason_;
  // True if the user was given the option to proceed through the
  // certificate chain error being reported.
  bool overridable_;
  // The time at which the interstitial was constructed.
  const base::Time interstitial_time_;
  // Helpful for recording metrics about cert reports.
  security_interstitials::MetricsHelper* metrics_helper_;

  DISALLOW_COPY_AND_ASSIGN(CertReportHelper);
};

#endif  // CHROME_BROWSER_SSL_CERT_REPORT_HELPER_H_
