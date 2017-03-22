// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/security_state.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "components/security_state/core/switches.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"

namespace security_state {

namespace {

// Do not change or reorder this enum, and add new values at the end. It is used
// in the MarkHttpAs histogram.
enum MarkHttpStatus {
  NEUTRAL /* deprecated */,
  NON_SECURE,
  HTTP_SHOW_WARNING_ON_SENSITIVE_FIELDS,
  LAST_STATUS
};

// If |switch_or_field_trial_group| corresponds to a valid
// MarkHttpAs group, sets |*level| and |*histogram_status| to the
// appropriate values and returns true. Otherwise, returns false.
bool GetSecurityLevelAndHistogramValueForNonSecureFieldTrial(
    std::string switch_or_field_trial_group,
    bool displayed_sensitive_input_on_http,
    SecurityLevel* level,
    MarkHttpStatus* histogram_status) {
  if (switch_or_field_trial_group != switches::kMarkHttpAsDangerous)
    return false;
  *level = DANGEROUS;
  *histogram_status = NON_SECURE;
  return true;
}

SecurityLevel GetSecurityLevelForNonSecureFieldTrial(
    bool displayed_sensitive_input_on_http) {
  std::string choice =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kMarkHttpAs);
  std::string group = base::FieldTrialList::FindFullName("MarkNonSecureAs");

  const char kEnumeration[] = "SSL.MarkHttpAsStatus";

  SecurityLevel level = NONE;
  MarkHttpStatus status;

  // If the command-line switch is set, then it takes precedence over
  // the field trial group.
  if (!GetSecurityLevelAndHistogramValueForNonSecureFieldTrial(
          choice, displayed_sensitive_input_on_http, &level, &status)) {
    if (!GetSecurityLevelAndHistogramValueForNonSecureFieldTrial(
            group, displayed_sensitive_input_on_http, &level, &status)) {
      status = HTTP_SHOW_WARNING_ON_SENSITIVE_FIELDS;
      level = displayed_sensitive_input_on_http
                  ? security_state::HTTP_SHOW_WARNING
                  : NONE;
    }
  }

  UMA_HISTOGRAM_ENUMERATION(kEnumeration, status, LAST_STATUS);
  return level;
}

ContentStatus GetContentStatus(bool displayed, bool ran) {
  if (ran && displayed)
    return CONTENT_STATUS_DISPLAYED_AND_RAN;
  if (ran)
    return CONTENT_STATUS_RAN;
  if (displayed)
    return CONTENT_STATUS_DISPLAYED;
  return CONTENT_STATUS_NONE;
}

SecurityLevel GetSecurityLevelForRequest(
    const VisibleSecurityState& visible_security_state,
    bool used_policy_installed_certificate,
    const IsOriginSecureCallback& is_origin_secure_callback,
    bool sha1_in_chain,
    ContentStatus mixed_content_status,
    ContentStatus content_with_cert_errors_status) {
  DCHECK(visible_security_state.connection_info_initialized ||
         visible_security_state.malicious_content_status !=
             MALICIOUS_CONTENT_STATUS_NONE);

  // Override the connection security information if the website failed the
  // browser's malware checks.
  if (visible_security_state.malicious_content_status !=
      MALICIOUS_CONTENT_STATUS_NONE) {
    return DANGEROUS;
  }

  const GURL url = visible_security_state.url;

  const bool is_cryptographic_with_certificate =
      (url.SchemeIsCryptographic() && visible_security_state.certificate);

  // Set the security level to DANGEROUS for major certificate errors.
  if (is_cryptographic_with_certificate &&
      net::IsCertStatusError(visible_security_state.cert_status) &&
      !net::IsCertStatusMinorError(visible_security_state.cert_status)) {
    return DANGEROUS;
  }

  // data: URLs don't define a secure context, and are a vector for spoofing.
  // Display a "Not secure" badge for all data URLs, regardless of whether
  // they show a password or credit card field.
  if (url.SchemeIs(url::kDataScheme))
    return SecurityLevel::HTTP_SHOW_WARNING;

  // Choose the appropriate security level for requests to HTTP and remaining
  // pseudo URLs (blob:, filesystem:). filesystem: is a standard scheme so does
  // not need to be explicitly listed here.
  // TODO(meacer): Remove special case for blob (crbug.com/684751).
  if (!is_cryptographic_with_certificate) {
    if (!is_origin_secure_callback.Run(url) &&
        (url.IsStandard() || url.SchemeIs(url::kBlobScheme))) {
      return GetSecurityLevelForNonSecureFieldTrial(
          visible_security_state.displayed_password_field_on_http ||
          visible_security_state.displayed_credit_card_field_on_http);
    }
    return NONE;
  }

  // Downgrade the security level for active insecure subresources.
  if (mixed_content_status == CONTENT_STATUS_RAN ||
      mixed_content_status == CONTENT_STATUS_DISPLAYED_AND_RAN ||
      content_with_cert_errors_status == CONTENT_STATUS_RAN ||
      content_with_cert_errors_status == CONTENT_STATUS_DISPLAYED_AND_RAN) {
    return kRanInsecureContentLevel;
  }

  // Report if there is a policy cert first, before reporting any other
  // authenticated-but-with-errors cases. A policy cert is a strong
  // indicator of a MITM being present (the enterprise), while the
  // other authenticated-but-with-errors indicate something may
  // be wrong, or may be wrong in the future, but is unclear now.
  if (used_policy_installed_certificate)
    return SECURE_WITH_POLICY_INSTALLED_CERT;

  // In most cases, SHA1 use is treated as a certificate error, in which case
  // DANGEROUS will have been returned above. If SHA1 was permitted by policy,
  // downgrade the security level to Neutral.
  if (sha1_in_chain)
    return NONE;

  // Active mixed content is handled above.
  DCHECK_NE(CONTENT_STATUS_RAN, mixed_content_status);
  DCHECK_NE(CONTENT_STATUS_DISPLAYED_AND_RAN, mixed_content_status);

  if (mixed_content_status == CONTENT_STATUS_DISPLAYED ||
      content_with_cert_errors_status == CONTENT_STATUS_DISPLAYED) {
    return kDisplayedInsecureContentLevel;
  }

  if (net::IsCertStatusError(visible_security_state.cert_status)) {
    // Major cert errors are handled above.
    DCHECK(net::IsCertStatusMinorError(visible_security_state.cert_status));
    return NONE;
  }

  if ((visible_security_state.cert_status & net::CERT_STATUS_IS_EV) &&
      visible_security_state.certificate) {
    return EV_SECURE;
  }
  return SECURE;
}

void SecurityInfoForRequest(
    const VisibleSecurityState& visible_security_state,
    bool used_policy_installed_certificate,
    const IsOriginSecureCallback& is_origin_secure_callback,
    SecurityInfo* security_info) {
  if (!visible_security_state.connection_info_initialized) {
    *security_info = SecurityInfo();
    security_info->malicious_content_status =
        visible_security_state.malicious_content_status;
    if (security_info->malicious_content_status !=
        MALICIOUS_CONTENT_STATUS_NONE) {
      security_info->security_level = GetSecurityLevelForRequest(
          visible_security_state, used_policy_installed_certificate,
          is_origin_secure_callback, false, CONTENT_STATUS_UNKNOWN,
          CONTENT_STATUS_UNKNOWN);
    }
    return;
  }
  security_info->certificate = visible_security_state.certificate;

  security_info->sha1_in_chain = visible_security_state.certificate &&
                                 (visible_security_state.cert_status &
                                  net::CERT_STATUS_SHA1_SIGNATURE_PRESENT);
  security_info->mixed_content_status =
      GetContentStatus(visible_security_state.displayed_mixed_content,
                       visible_security_state.ran_mixed_content);
  security_info->content_with_cert_errors_status = GetContentStatus(
      visible_security_state.displayed_content_with_cert_errors,
      visible_security_state.ran_content_with_cert_errors);
  security_info->security_bits = visible_security_state.security_bits;
  security_info->connection_status = visible_security_state.connection_status;
  security_info->key_exchange_group = visible_security_state.key_exchange_group;
  security_info->cert_status = visible_security_state.cert_status;
  security_info->scheme_is_cryptographic =
      visible_security_state.url.SchemeIsCryptographic();
  security_info->obsolete_ssl_status =
      net::ObsoleteSSLStatus(security_info->connection_status);
  security_info->pkp_bypassed = visible_security_state.pkp_bypassed;
  security_info->sct_verify_statuses =
      visible_security_state.sct_verify_statuses;

  security_info->malicious_content_status =
      visible_security_state.malicious_content_status;

  security_info->displayed_password_field_on_http =
      visible_security_state.displayed_password_field_on_http;
  security_info->displayed_credit_card_field_on_http =
      visible_security_state.displayed_credit_card_field_on_http;
  if (visible_security_state.certificate) {
    security_info->cert_missing_subject_alt_name =
        !visible_security_state.certificate->GetSubjectAltName(nullptr,
                                                               nullptr);
  }

  security_info->security_level = GetSecurityLevelForRequest(
      visible_security_state, used_policy_installed_certificate,
      is_origin_secure_callback, security_info->sha1_in_chain,
      security_info->mixed_content_status,
      security_info->content_with_cert_errors_status);
}

}  // namespace

const base::Feature kHttpFormWarningFeature{"HttpFormWarning",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

SecurityInfo::SecurityInfo()
    : security_level(NONE),
      malicious_content_status(MALICIOUS_CONTENT_STATUS_NONE),
      sha1_in_chain(false),
      mixed_content_status(CONTENT_STATUS_NONE),
      content_with_cert_errors_status(CONTENT_STATUS_NONE),
      scheme_is_cryptographic(false),
      cert_status(0),
      security_bits(-1),
      connection_status(0),
      key_exchange_group(0),
      obsolete_ssl_status(net::OBSOLETE_SSL_NONE),
      pkp_bypassed(false),
      displayed_password_field_on_http(false),
      displayed_credit_card_field_on_http(false),
      cert_missing_subject_alt_name(false) {}

SecurityInfo::~SecurityInfo() {}

void GetSecurityInfo(
    std::unique_ptr<VisibleSecurityState> visible_security_state,
    bool used_policy_installed_certificate,
    IsOriginSecureCallback is_origin_secure_callback,
    SecurityInfo* result) {
  SecurityInfoForRequest(*visible_security_state,
                         used_policy_installed_certificate,
                         is_origin_secure_callback, result);
}

bool IsHttpWarningInFormEnabled() {
  return base::FeatureList::IsEnabled(kHttpFormWarningFeature);
}

VisibleSecurityState::VisibleSecurityState()
    : malicious_content_status(MALICIOUS_CONTENT_STATUS_NONE),
      connection_info_initialized(false),
      cert_status(0),
      connection_status(0),
      key_exchange_group(0),
      security_bits(-1),
      displayed_mixed_content(false),
      ran_mixed_content(false),
      displayed_content_with_cert_errors(false),
      ran_content_with_cert_errors(false),
      pkp_bypassed(false),
      displayed_password_field_on_http(false),
      displayed_credit_card_field_on_http(false) {}

VisibleSecurityState::~VisibleSecurityState() {}

bool VisibleSecurityState::operator==(const VisibleSecurityState& other) const {
  return (url == other.url &&
          malicious_content_status == other.malicious_content_status &&
          !!certificate == !!other.certificate &&
          (certificate ? certificate->Equals(other.certificate.get()) : true) &&
          connection_status == other.connection_status &&
          key_exchange_group == other.key_exchange_group &&
          security_bits == other.security_bits &&
          sct_verify_statuses == other.sct_verify_statuses &&
          displayed_mixed_content == other.displayed_mixed_content &&
          ran_mixed_content == other.ran_mixed_content &&
          displayed_content_with_cert_errors ==
              other.displayed_content_with_cert_errors &&
          ran_content_with_cert_errors == other.ran_content_with_cert_errors &&
          pkp_bypassed == other.pkp_bypassed &&
          displayed_password_field_on_http ==
              other.displayed_password_field_on_http &&
          displayed_credit_card_field_on_http ==
              other.displayed_credit_card_field_on_http);
}

}  // namespace security_state
