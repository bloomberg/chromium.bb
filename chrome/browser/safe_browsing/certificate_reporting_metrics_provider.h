// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_METRICS_PROVIDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_METRICS_PROVIDER_H_

#include "base/macros.h"
#include "components/metrics/metrics_provider.h"

// When user metrics are about to be uploaded,
// CertificateReportingMetricsProvider looks up the CertificateReportingService
// for the current profile and tells it to record its metrics before the upload
// occurs.
class CertificateReportingMetricsProvider : public metrics::MetricsProvider {
 public:
  CertificateReportingMetricsProvider();
  ~CertificateReportingMetricsProvider() override;

  // metrics:MetricsProvider:
  void ProvideGeneralMetrics(
      metrics::ChromeUserMetricsExtension* unused) override;
};

#endif  // CHROME_BROWSER_SAFE_BROWSING_CERTIFICATE_REPORTING_METRICS_PROVIDER_H_
