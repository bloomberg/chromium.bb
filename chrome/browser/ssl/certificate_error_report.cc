// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/certificate_error_report.h"

#include <vector>

#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/ssl/cert_logger.pb.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_info.h"

namespace {

void AddCertStatusToReportErrors(net::CertStatus cert_status,
                                 CertLoggerRequest* report) {
  if (cert_status & net::CERT_STATUS_REVOKED)
    report->add_cert_error(CertLoggerRequest::ERR_CERT_REVOKED);
  if (cert_status & net::CERT_STATUS_INVALID)
    report->add_cert_error(CertLoggerRequest::ERR_CERT_INVALID);
  if (cert_status & net::CERT_STATUS_PINNED_KEY_MISSING)
    report->add_cert_error(
        CertLoggerRequest::ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN);
  if (cert_status & net::CERT_STATUS_AUTHORITY_INVALID)
    report->add_cert_error(CertLoggerRequest::ERR_CERT_AUTHORITY_INVALID);
  if (cert_status & net::CERT_STATUS_COMMON_NAME_INVALID)
    report->add_cert_error(CertLoggerRequest::ERR_CERT_COMMON_NAME_INVALID);
  if (cert_status & net::CERT_STATUS_NON_UNIQUE_NAME)
    report->add_cert_error(CertLoggerRequest::ERR_CERT_NON_UNIQUE_NAME);
  if (cert_status & net::CERT_STATUS_NAME_CONSTRAINT_VIOLATION)
    report->add_cert_error(
        CertLoggerRequest::ERR_CERT_NAME_CONSTRAINT_VIOLATION);
  if (cert_status & net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM)
    report->add_cert_error(
        CertLoggerRequest::ERR_CERT_WEAK_SIGNATURE_ALGORITHM);
  if (cert_status & net::CERT_STATUS_WEAK_KEY)
    report->add_cert_error(CertLoggerRequest::ERR_CERT_WEAK_KEY);
  if (cert_status & net::CERT_STATUS_DATE_INVALID)
    report->add_cert_error(CertLoggerRequest::ERR_CERT_DATE_INVALID);
  if (cert_status & net::CERT_STATUS_VALIDITY_TOO_LONG)
    report->add_cert_error(CertLoggerRequest::ERR_CERT_VALIDITY_TOO_LONG);
  if (cert_status & net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION)
    report->add_cert_error(
        CertLoggerRequest::ERR_CERT_UNABLE_TO_CHECK_REVOCATION);
  if (cert_status & net::CERT_STATUS_NO_REVOCATION_MECHANISM)
    report->add_cert_error(CertLoggerRequest::ERR_CERT_NO_REVOCATION_MECHANISM);
}
}  // namespace

CertificateErrorReport::CertificateErrorReport()
    : cert_report_(new CertLoggerRequest()) {
}

CertificateErrorReport::CertificateErrorReport(const std::string& hostname,
                                               const net::SSLInfo& ssl_info)
    : cert_report_(new CertLoggerRequest()) {
  base::Time now = base::Time::Now();
  cert_report_->set_time_usec(now.ToInternalValue());
  cert_report_->set_hostname(hostname);

  std::vector<std::string> pem_encoded_chain;
  if (!ssl_info.cert->GetPEMEncodedChain(&pem_encoded_chain)) {
    LOG(ERROR) << "Could not get PEM encoded chain.";
  }

  std::string* cert_chain = cert_report_->mutable_cert_chain();
  for (size_t i = 0; i < pem_encoded_chain.size(); ++i)
    cert_chain->append(pem_encoded_chain[i]);

  cert_report_->add_pin(ssl_info.pinning_failure_log);

  AddCertStatusToReportErrors(ssl_info.cert_status, cert_report_.get());
}

CertificateErrorReport::~CertificateErrorReport() {
}

bool CertificateErrorReport::InitializeFromString(
    const std::string& serialized_report) {
  return cert_report_->ParseFromString(serialized_report);
}

bool CertificateErrorReport::Serialize(std::string* output) const {
  return cert_report_->SerializeToString(output);
}

void CertificateErrorReport::SetInterstitialInfo(
    const InterstitialReason& interstitial_reason,
    const ProceedDecision& proceed_decision,
    const Overridable& overridable) {
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
  }

  interstitial_info->set_user_proceeded(proceed_decision == USER_PROCEEDED);
  interstitial_info->set_overridable(overridable == INTERSTITIAL_OVERRIDABLE);
}

const std::string& CertificateErrorReport::hostname() const {
  return cert_report_->hostname();
}
