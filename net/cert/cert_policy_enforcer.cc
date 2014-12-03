// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_policy_enforcer.h"

#include <algorithm>

#include "base/build_time.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "net/cert/ct_ev_whitelist.h"
#include "net/cert/ct_verify_result.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace {

bool IsEmbeddedSCT(const scoped_refptr<ct::SignedCertificateTimestamp>& sct) {
  return sct->origin == ct::SignedCertificateTimestamp::SCT_EMBEDDED;
}

// Returns true if the current build is recent enough to ensure that
// built-in security information (e.g. CT Logs) is fresh enough.
// TODO(eranm): Move to base or net/base
bool IsBuildTimely() {
#if defined(DONT_EMBED_BUILD_METADATA) && !defined(OFFICIAL_BUILD)
  return true;
#else
  const base::Time build_time = base::GetBuildTime();
  // We consider built-in information to be timely for 10 weeks.
  return (base::Time::Now() - build_time).InDays() < 70 /* 10 weeks */;
#endif
}

uint32_t ApproximateMonthDifference(const base::Time& start,
                                    const base::Time& end) {
  base::Time::Exploded exploded_start;
  base::Time::Exploded exploded_expiry;
  start.UTCExplode(&exploded_start);
  end.UTCExplode(&exploded_expiry);
  uint32_t month_diff = (exploded_expiry.year - exploded_start.year) * 12 +
                        (exploded_expiry.month - exploded_start.month);

  // Add any remainder as a full month.
  if (exploded_expiry.day_of_month > exploded_start.day_of_month)
    ++month_diff;

  return month_diff;
}

enum CTComplianceStatus {
  CT_NOT_COMPLIANT = 0,
  CT_IN_WHITELIST = 1,
  CT_ENOUGH_SCTS = 2,
  CT_COMPLIANCE_MAX,
};

void LogCTComplianceStatusToUMA(CTComplianceStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Net.SSL_EVCertificateCTCompliance", status,
                            CT_COMPLIANCE_MAX);
}

}  // namespace

CertPolicyEnforcer::CertPolicyEnforcer(size_t num_ct_logs,
                                       bool require_ct_for_ev)
    : num_ct_logs_(num_ct_logs), require_ct_for_ev_(require_ct_for_ev) {
}

CertPolicyEnforcer::~CertPolicyEnforcer() {
}

bool CertPolicyEnforcer::DoesConformToCTEVPolicy(
    X509Certificate* cert,
    const ct::EVCertsWhitelist* ev_whitelist,
    const ct::CTVerifyResult& ct_result) {
  if (!require_ct_for_ev_)
    return true;

  if (!IsBuildTimely())
    return false;

  if (IsCertificateInWhitelist(cert, ev_whitelist)) {
    LogCTComplianceStatusToUMA(CT_IN_WHITELIST);
    return true;
  }

  if (HasRequiredNumberOfSCTs(cert, ct_result)) {
    LogCTComplianceStatusToUMA(CT_ENOUGH_SCTS);
    return true;
  }

  LogCTComplianceStatusToUMA(CT_NOT_COMPLIANT);
  return false;
}

bool CertPolicyEnforcer::IsCertificateInWhitelist(
    X509Certificate* cert,
    const ct::EVCertsWhitelist* ev_whitelist) {
  bool cert_in_ev_whitelist = false;
  if (ev_whitelist && ev_whitelist->IsValid()) {
    const SHA256HashValue fingerprint(
        X509Certificate::CalculateFingerprint256(cert->os_cert_handle()));

    std::string truncated_fp =
        std::string(reinterpret_cast<const char*>(fingerprint.data), 8);
    cert_in_ev_whitelist = ev_whitelist->ContainsCertificateHash(truncated_fp);

    UMA_HISTOGRAM_BOOLEAN("Net.SSL_EVCertificateInWhitelist",
                          cert_in_ev_whitelist);
  }
  return cert_in_ev_whitelist;
}

bool CertPolicyEnforcer::HasRequiredNumberOfSCTs(
    X509Certificate* cert,
    const ct::CTVerifyResult& ct_result) {
  // TODO(eranm): Count the number of *independent* SCTs once the information
  // about log operators is available, crbug.com/425174
  size_t num_valid_scts = ct_result.verified_scts.size();
  size_t num_embedded_scts =
      std::count_if(ct_result.verified_scts.begin(),
                    ct_result.verified_scts.end(), IsEmbeddedSCT);

  size_t num_non_embedded_scts = num_valid_scts - num_embedded_scts;
  // If at least two valid SCTs were delivered by means other than embedding
  // (i.e. in a TLS extension or OCSP), then the certificate conforms to bullet
  // number 3 of the "Qualifying Certificate" section of the CT/EV policy.
  if (num_non_embedded_scts >= 2)
    return true;

  if (cert->valid_start().is_null() || cert->valid_expiry().is_null() ||
      cert->valid_start().is_max() || cert->valid_expiry().is_max()) {
    // Will not be able to calculate the certificate's validity period.
    return false;
  }

  uint32_t expiry_in_months_approx =
      ApproximateMonthDifference(cert->valid_start(), cert->valid_expiry());

  // For embedded SCTs, if the certificate has the number of SCTs specified in
  // table 1 of the "Qualifying Certificate" section of the CT/EV policy, then
  // it qualifies.
  size_t num_required_embedded_scts;
  if (expiry_in_months_approx > 39) {
    num_required_embedded_scts = 5;
  } else if (expiry_in_months_approx > 27) {
    num_required_embedded_scts = 4;
  } else if (expiry_in_months_approx >= 15) {
    num_required_embedded_scts = 3;
  } else {
    num_required_embedded_scts = 2;
  }

  size_t min_acceptable_logs = std::max(num_ct_logs_, static_cast<size_t>(2u));
  return num_embedded_scts >=
         std::min(num_required_embedded_scts, min_acceptable_logs);
}

}  // namespace net
