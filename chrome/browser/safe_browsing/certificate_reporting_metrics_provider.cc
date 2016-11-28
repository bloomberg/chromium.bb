// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_metrics_provider.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"

CertificateReportingMetricsProvider::CertificateReportingMetricsProvider() {}

CertificateReportingMetricsProvider::~CertificateReportingMetricsProvider() {}

void CertificateReportingMetricsProvider::ProvideGeneralMetrics(
    metrics::ChromeUserMetricsExtension* unused) {}
