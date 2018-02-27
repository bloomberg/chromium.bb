// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_reporting/error_report.h"

#include <vector>

#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/network_time/network_time_tracker.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"

#if defined(OS_ANDROID)
#include "net/cert/cert_verify_proc_android.h"
#endif

using network_time::NetworkTimeTracker;

namespace certificate_reporting {

namespace {

void AddCertStatusToReportErrors(net::CertStatus cert_status,
                                 CertLoggerRequest* report) {
#define COPY_CERT_STATUS(error) RENAME_CERT_STATUS(error, CERT_##error)
#define RENAME_CERT_STATUS(status_error, logger_error) \
  if (cert_status & net::CERT_STATUS_##status_error)   \
    report->add_cert_error(CertLoggerRequest::ERR_##logger_error);

  COPY_CERT_STATUS(REVOKED)
  COPY_CERT_STATUS(INVALID)
  RENAME_CERT_STATUS(PINNED_KEY_MISSING, SSL_PINNED_KEY_NOT_IN_CERT_CHAIN)
  COPY_CERT_STATUS(AUTHORITY_INVALID)
  COPY_CERT_STATUS(COMMON_NAME_INVALID)
  COPY_CERT_STATUS(NON_UNIQUE_NAME)
  COPY_CERT_STATUS(NAME_CONSTRAINT_VIOLATION)
  COPY_CERT_STATUS(WEAK_SIGNATURE_ALGORITHM)
  COPY_CERT_STATUS(WEAK_KEY)
  COPY_CERT_STATUS(DATE_INVALID)
  COPY_CERT_STATUS(VALIDITY_TOO_LONG)
  COPY_CERT_STATUS(UNABLE_TO_CHECK_REVOCATION)
  COPY_CERT_STATUS(NO_REVOCATION_MECHANISM)
  RENAME_CERT_STATUS(CERTIFICATE_TRANSPARENCY_REQUIRED,
                     CERTIFICATE_TRANSPARENCY_REQUIRED)
  COPY_CERT_STATUS(SYMANTEC_LEGACY)

#undef RENAME_CERT_STATUS
#undef COPY_CERT_STATUS
}

bool CertificateChainToString(scoped_refptr<net::X509Certificate> cert,
                              std::string* result) {
  std::vector<std::string> pem_encoded_chain;
  if (!cert->GetPEMEncodedChain(&pem_encoded_chain))
    return false;

  *result = base::StrCat(pem_encoded_chain);
  return true;
}

}  // namespace

ErrorReport::ErrorReport() : cert_report_(new CertLoggerRequest()) {}

ErrorReport::ErrorReport(const std::string& hostname,
                         const net::SSLInfo& ssl_info)
    : cert_report_(new CertLoggerRequest()) {
  base::Time now = base::Time::Now();
  cert_report_->set_time_usec(now.ToInternalValue());
  cert_report_->set_hostname(hostname);

  if (!CertificateChainToString(ssl_info.cert,
                                cert_report_->mutable_cert_chain())) {
    LOG(ERROR) << "Could not get PEM encoded chain.";
  }

  if (ssl_info.unverified_cert &&
      !CertificateChainToString(
          ssl_info.unverified_cert,
          cert_report_->mutable_unverified_cert_chain())) {
    LOG(ERROR) << "Could not get PEM encoded unverified certificate chain.";
  }

  cert_report_->add_pin(ssl_info.pinning_failure_log);
  cert_report_->set_is_issued_by_known_root(ssl_info.is_issued_by_known_root);

  AddCertStatusToReportErrors(ssl_info.cert_status, cert_report_.get());

#if defined(OS_ANDROID)
  CertLoggerFeaturesInfo* features_info = cert_report_->mutable_features_info();
  features_info->set_android_aia_fetching_status(
      CertLoggerFeaturesInfo::ANDROID_AIA_FETCHING_ENABLED);
#endif
}

ErrorReport::~ErrorReport() {}

bool ErrorReport::InitializeFromString(const std::string& serialized_report) {
  return cert_report_->ParseFromString(serialized_report);
}

bool ErrorReport::Serialize(std::string* output) const {
  return cert_report_->SerializeToString(output);
}

void ErrorReport::SetInterstitialInfo(
    const InterstitialReason& interstitial_reason,
    const ProceedDecision& proceed_decision,
    const Overridable& overridable,
    const base::Time& interstitial_time) {
  CertLoggerInterstitialInfo* interstitial_info =
      cert_report_->mutable_interstitial_info();

  switch (interstitial_reason) {
    case INTERSTITIAL_SSL:
      interstitial_info->set_interstitial_reason(
          CertLoggerInterstitialInfo::INTERSTITIAL_SSL);
      break;
    case INTERSTITIAL_CAPTIVE_PORTAL:
      interstitial_info->set_interstitial_reason(
          CertLoggerInterstitialInfo::INTERSTITIAL_CAPTIVE_PORTAL);
      break;
    case INTERSTITIAL_CLOCK:
      interstitial_info->set_interstitial_reason(
          CertLoggerInterstitialInfo::INTERSTITIAL_CLOCK);
      break;
    case INTERSTITIAL_SUPERFISH:
      interstitial_info->set_interstitial_reason(
          CertLoggerInterstitialInfo::INTERSTITIAL_SUPERFISH);
      break;
    case INTERSTITIAL_MITM_SOFTWARE:
      interstitial_info->set_interstitial_reason(
          CertLoggerInterstitialInfo::INTERSTITIAL_MITM_SOFTWARE);
      break;
  }

  interstitial_info->set_user_proceeded(proceed_decision == USER_PROCEEDED);
  interstitial_info->set_overridable(overridable == INTERSTITIAL_OVERRIDABLE);
  interstitial_info->set_interstitial_created_time_usec(
      interstitial_time.ToInternalValue());
}

void ErrorReport::AddNetworkTimeInfo(
    const NetworkTimeTracker* network_time_tracker) {
  CertLoggerFeaturesInfo* features_info = cert_report_->mutable_features_info();
  CertLoggerFeaturesInfo::NetworkTimeQueryingInfo* network_time_info =
      features_info->mutable_network_time_querying_info();
  network_time_info->set_network_time_queries_enabled(
      network_time_tracker->AreTimeFetchesEnabled());
  NetworkTimeTracker::FetchBehavior behavior =
      network_time_tracker->GetFetchBehavior();
  CertLoggerFeaturesInfo::NetworkTimeQueryingInfo::NetworkTimeFetchBehavior
      report_behavior = CertLoggerFeaturesInfo::NetworkTimeQueryingInfo::
          NETWORK_TIME_FETCHES_UNKNOWN;

  switch (behavior) {
    case NetworkTimeTracker::FETCH_BEHAVIOR_UNKNOWN:
      report_behavior = CertLoggerFeaturesInfo::NetworkTimeQueryingInfo::
          NETWORK_TIME_FETCHES_UNKNOWN;
      break;
    case NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY:
      report_behavior = CertLoggerFeaturesInfo::NetworkTimeQueryingInfo::
          NETWORK_TIME_FETCHES_BACKGROUND_ONLY;
      break;
    case NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY:
      report_behavior = CertLoggerFeaturesInfo::NetworkTimeQueryingInfo::
          NETWORK_TIME_FETCHES_ON_DEMAND_ONLY;
      break;
    case NetworkTimeTracker::FETCHES_IN_BACKGROUND_AND_ON_DEMAND:
      report_behavior = CertLoggerFeaturesInfo::NetworkTimeQueryingInfo::
          NETWORK_TIME_FETCHES_IN_BACKGROUND_AND_ON_DEMAND;
      break;
  }
  network_time_info->set_network_time_query_behavior(report_behavior);
}

void ErrorReport::AddChromeChannel(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::STABLE:
      cert_report_->set_chrome_channel(
          CertLoggerRequest::CHROME_CHANNEL_STABLE);
      break;

    case version_info::Channel::BETA:
      cert_report_->set_chrome_channel(CertLoggerRequest::CHROME_CHANNEL_BETA);
      break;

    case version_info::Channel::CANARY:
      cert_report_->set_chrome_channel(
          CertLoggerRequest::CHROME_CHANNEL_CANARY);
      break;

    case version_info::Channel::DEV:
      cert_report_->set_chrome_channel(CertLoggerRequest::CHROME_CHANNEL_DEV);
      break;

    case version_info::Channel::UNKNOWN:
      cert_report_->set_chrome_channel(
          CertLoggerRequest::CHROME_CHANNEL_UNKNOWN);
      break;
  }
}

void ErrorReport::SetIsEnterpriseManaged(bool is_enterprise_managed) {
  cert_report_->set_is_enterprise_managed(is_enterprise_managed);
}

void ErrorReport::SetIsRetryUpload(bool is_retry_upload) {
  cert_report_->set_is_retry_upload(is_retry_upload);
}

const std::string& ErrorReport::hostname() const {
  return cert_report_->hostname();
}

CertLoggerRequest::ChromeChannel ErrorReport::chrome_channel() const {
  return cert_report_->chrome_channel();
}

bool ErrorReport::is_enterprise_managed() const {
  return cert_report_->is_enterprise_managed();
}

bool ErrorReport::is_retry_upload() const {
  return cert_report_->is_retry_upload();
}

}  // namespace certificate_reporting
