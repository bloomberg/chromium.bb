// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_policy_enforcer.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/build_time.h"
#include "base/callback_helpers.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "net/cert/ct_ev_whitelist.h"
#include "net/cert/ct_known_logs.h"
#include "net/cert/ct_verify_result.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_certificate_net_log_param.h"
#include "net/log/net_log.h"

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

bool IsGoogleIssuedSCT(
    const scoped_refptr<ct::SignedCertificateTimestamp>& sct) {
  return ct::IsLogOperatedByGoogle(sct->log_id);
}

// Returns a rounded-down months difference of |start| and |end|,
// together with an indication of whether the last month was
// a full month, because the range starts specified in the policy
// are not consistent in terms of including the range start value.
void RoundedDownMonthDifference(const base::Time& start,
                                const base::Time& end,
                                size_t* rounded_months_difference,
                                bool* has_partial_month) {
  DCHECK(rounded_months_difference);
  DCHECK(has_partial_month);
  base::Time::Exploded exploded_start;
  base::Time::Exploded exploded_expiry;
  start.UTCExplode(&exploded_start);
  end.UTCExplode(&exploded_expiry);
  if (end < start) {
    *rounded_months_difference = 0;
    *has_partial_month = false;
  }

  *has_partial_month = true;
  uint32_t month_diff = (exploded_expiry.year - exploded_start.year) * 12 +
                        (exploded_expiry.month - exploded_start.month);
  if (exploded_expiry.day_of_month < exploded_start.day_of_month)
    --month_diff;
  else if (exploded_expiry.day_of_month == exploded_start.day_of_month)
    *has_partial_month = false;

  *rounded_months_difference = month_diff;
}

bool HasRequiredNumberOfSCTs(const X509Certificate& cert,
                             const ct::CTVerifyResult& ct_result) {
  size_t num_valid_scts = ct_result.verified_scts.size();
  size_t num_embedded_scts = base::checked_cast<size_t>(
      std::count_if(ct_result.verified_scts.begin(),
                    ct_result.verified_scts.end(), IsEmbeddedSCT));

  size_t num_non_embedded_scts = num_valid_scts - num_embedded_scts;
  // If at least two valid SCTs were delivered by means other than embedding
  // (i.e. in a TLS extension or OCSP), then the certificate conforms to bullet
  // number 3 of the "Qualifying Certificate" section of the CT/EV policy.
  if (num_non_embedded_scts >= 2)
    return true;

  if (cert.valid_start().is_null() || cert.valid_expiry().is_null() ||
      cert.valid_start().is_max() || cert.valid_expiry().is_max()) {
    // Will not be able to calculate the certificate's validity period.
    return false;
  }

  size_t lifetime;
  bool has_partial_month;
  RoundedDownMonthDifference(cert.valid_start(), cert.valid_expiry(), &lifetime,
                             &has_partial_month);

  // For embedded SCTs, if the certificate has the number of SCTs specified in
  // table 1 of the "Qualifying Certificate" section of the CT/EV policy, then
  // it qualifies.
  size_t num_required_embedded_scts;
  if (lifetime > 39 || (lifetime == 39 && has_partial_month)) {
    num_required_embedded_scts = 5;
  } else if (lifetime > 27 || (lifetime == 27 && has_partial_month)) {
    num_required_embedded_scts = 4;
  } else if (lifetime >= 15) {
    num_required_embedded_scts = 3;
  } else {
    num_required_embedded_scts = 2;
  }

  return num_embedded_scts >= num_required_embedded_scts;
}

// Returns true if |verified_scts| contains SCTs from at least one log that is
// operated by Google and at least one log that is not operated by Google. This
// is required for SCTs after July 1st, 2015, as documented at
// http://dev.chromium.org/Home/chromium-security/root-ca-policy/EVCTPlanMay2015edition.pdf
bool HasEnoughDiverseSCTs(const ct::SCTList& verified_scts) {
  size_t num_google_issued_scts = base::checked_cast<size_t>(std::count_if(
      verified_scts.begin(), verified_scts.end(), IsGoogleIssuedSCT));
  return (num_google_issued_scts > 0) &&
         (verified_scts.size() != num_google_issued_scts);
}

enum CTComplianceStatus {
  CT_NOT_COMPLIANT = 0,
  CT_IN_WHITELIST = 1,
  CT_ENOUGH_SCTS = 2,
  CT_NOT_ENOUGH_DIVERSE_SCTS = 3,
  CT_COMPLIANCE_MAX,
};

const char* ComplianceStatusToString(CTComplianceStatus status) {
  switch (status) {
    case CT_NOT_COMPLIANT:
      return "NOT_COMPLIANT";
      break;
    case CT_IN_WHITELIST:
      return "WHITELISTED";
      break;
    case CT_ENOUGH_SCTS:
      return "ENOUGH_SCTS";
      break;
    case CT_NOT_ENOUGH_DIVERSE_SCTS:
      return "NOT_ENOUGH_DIVERSE_SCTS";
      break;
    case CT_COMPLIANCE_MAX:
      break;
  }

  return "unknown";
}

enum EVWhitelistStatus {
  EV_WHITELIST_NOT_PRESENT = 0,
  EV_WHITELIST_INVALID = 1,
  EV_WHITELIST_VALID = 2,
  EV_WHITELIST_MAX,
};

void LogCTComplianceStatusToUMA(CTComplianceStatus status,
                                const ct::EVCertsWhitelist* ev_whitelist) {
  UMA_HISTOGRAM_ENUMERATION("Net.SSL_EVCertificateCTCompliance", status,
                            CT_COMPLIANCE_MAX);
  if (status == CT_NOT_COMPLIANT) {
    EVWhitelistStatus ev_whitelist_status = EV_WHITELIST_NOT_PRESENT;
    if (ev_whitelist != NULL) {
      if (ev_whitelist->IsValid())
        ev_whitelist_status = EV_WHITELIST_VALID;
      else
        ev_whitelist_status = EV_WHITELIST_INVALID;
    }

    UMA_HISTOGRAM_ENUMERATION("Net.SSL_EVWhitelistValidityForNonCompliantCert",
                              ev_whitelist_status, EV_WHITELIST_MAX);
  }
}

struct ComplianceDetails {
  ComplianceDetails()
      : ct_presence_required(false),
        build_timely(false),
        status(CT_NOT_COMPLIANT) {}

  // Whether enforcement of the policy was required or not.
  bool ct_presence_required;
  // Whether the build is not older than 10 weeks. The value is meaningful only
  // if |ct_presence_required| is true.
  bool build_timely;
  // Compliance status - meaningful only if |ct_presence_required| and
  // |build_timely| are true.
  CTComplianceStatus status;
  // EV whitelist version.
  base::Version whitelist_version;
};

scoped_ptr<base::Value> NetLogComplianceCheckResultCallback(
    X509Certificate* cert,
    ComplianceDetails* details,
    NetLogCaptureMode capture_mode) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->Set("certificate", NetLogX509CertificateCallback(cert, capture_mode));
  dict->SetBoolean("policy_enforcement_required",
                   details->ct_presence_required);
  if (details->ct_presence_required) {
    dict->SetBoolean("build_timely", details->build_timely);
    if (details->build_timely) {
      dict->SetString("ct_compliance_status",
                      ComplianceStatusToString(details->status));
      if (details->whitelist_version.IsValid())
        dict->SetString("ev_whitelist_version",
                        details->whitelist_version.GetString());
    }
  }
  return std::move(dict);
}

// Returns true if all SCTs in |verified_scts| were issued on, or after, the
// date specified in kDiverseSCTRequirementStartDate
bool AllSCTsPastDistinctSCTRequirementEnforcementDate(
    const ct::SCTList& verified_scts) {
  // The date when diverse SCTs requirement is effective from.
  // 2015-07-01 00:00:00 UTC.
  base::Time kDiverseSCTRequirementStartDate =
      base::Time::FromInternalValue(13080182400000000);

  for (const auto& it : verified_scts) {
    if (it->timestamp < kDiverseSCTRequirementStartDate)
      return false;
  }

  return true;
}

bool IsCertificateInWhitelist(const X509Certificate& cert,
                              const ct::EVCertsWhitelist* ev_whitelist) {
  bool cert_in_ev_whitelist = false;
  if (ev_whitelist && ev_whitelist->IsValid()) {
    const SHA256HashValue fingerprint(
        X509Certificate::CalculateFingerprint256(cert.os_cert_handle()));

    std::string truncated_fp =
        std::string(reinterpret_cast<const char*>(fingerprint.data), 8);
    cert_in_ev_whitelist = ev_whitelist->ContainsCertificateHash(truncated_fp);

    UMA_HISTOGRAM_BOOLEAN("Net.SSL_EVCertificateInWhitelist",
                          cert_in_ev_whitelist);
  }
  return cert_in_ev_whitelist;
}

void CheckCTEVPolicyCompliance(X509Certificate* cert,
                               const ct::EVCertsWhitelist* ev_whitelist,
                               const ct::CTVerifyResult& ct_result,
                               ComplianceDetails* result) {
  result->ct_presence_required = true;

  if (!IsBuildTimely())
    return;
  result->build_timely = true;

  if (ev_whitelist && ev_whitelist->IsValid())
    result->whitelist_version = ev_whitelist->Version();

  if (IsCertificateInWhitelist(*cert, ev_whitelist)) {
    result->status = CT_IN_WHITELIST;
    return;
  }

  if (!HasRequiredNumberOfSCTs(*cert, ct_result)) {
    result->status = CT_NOT_COMPLIANT;
    return;
  }

  if (AllSCTsPastDistinctSCTRequirementEnforcementDate(
          ct_result.verified_scts) &&
      !HasEnoughDiverseSCTs(ct_result.verified_scts)) {
    result->status = CT_NOT_ENOUGH_DIVERSE_SCTS;
    return;
  }

  result->status = CT_ENOUGH_SCTS;
}

}  // namespace

bool CTPolicyEnforcer::DoesConformToCTEVPolicy(
    X509Certificate* cert,
    const ct::EVCertsWhitelist* ev_whitelist,
    const ct::CTVerifyResult& ct_result,
    const BoundNetLog& net_log) {
  ComplianceDetails details;

  CheckCTEVPolicyCompliance(cert, ev_whitelist, ct_result, &details);

  NetLog::ParametersCallback net_log_callback =
      base::Bind(&NetLogComplianceCheckResultCallback, base::Unretained(cert),
                 base::Unretained(&details));

  net_log.AddEvent(NetLog::TYPE_EV_CERT_CT_COMPLIANCE_CHECKED,
                   net_log_callback);

  if (!details.ct_presence_required)
    return true;

  if (!details.build_timely)
    return false;

  LogCTComplianceStatusToUMA(details.status, ev_whitelist);

  if (details.status == CT_IN_WHITELIST || details.status == CT_ENOUGH_SCTS)
    return true;

  return false;
}

}  // namespace net
