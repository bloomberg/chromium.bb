// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/security_state_model.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "components/security_state/security_state_model_client.h"
#include "components/security_state/switches.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"

namespace security_state {

namespace {

// Do not change or reorder this enum, and add new values at the end. It is used
// in the MarkHttpAs histogram.
enum MarkHttpStatus { NEUTRAL, NON_SECURE, HTTP_SHOW_WARNING, LAST_STATUS };

// If |switch_or_field_trial_group| corresponds to a valid
// MarkHttpAs group, sets |*level| and |*histogram_status| to the
// appropriate values and returns true. Otherwise, returns false.
bool GetSecurityLevelAndHistogramValueForNonSecureFieldTrial(
    std::string switch_or_field_trial_group,
    bool displayed_sensitive_input_on_http,
    SecurityStateModel::SecurityLevel* level,
    MarkHttpStatus* histogram_status) {
  if (switch_or_field_trial_group == switches::kMarkHttpAsNeutral) {
    *level = SecurityStateModel::NONE;
    *histogram_status = NEUTRAL;
    return true;
  }

  if (switch_or_field_trial_group == switches::kMarkHttpAsDangerous) {
    *level = SecurityStateModel::DANGEROUS;
    *histogram_status = NON_SECURE;
    return true;
  }

  if (switch_or_field_trial_group ==
      switches::kMarkHttpWithPasswordsOrCcWithChip) {
    if (displayed_sensitive_input_on_http) {
      *level = SecurityStateModel::HTTP_SHOW_WARNING;
      *histogram_status = HTTP_SHOW_WARNING;
    } else {
      *level = SecurityStateModel::NONE;
      *histogram_status = NEUTRAL;
    }
    return true;
  }

  return false;
}

SecurityStateModel::SecurityLevel GetSecurityLevelForNonSecureFieldTrial(
    bool displayed_sensitive_input_on_http) {
  std::string choice =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kMarkHttpAs);
  std::string group = base::FieldTrialList::FindFullName("MarkNonSecureAs");

  const char kEnumeration[] = "MarkHttpAs";

  SecurityStateModel::SecurityLevel level = SecurityStateModel::NONE;
  MarkHttpStatus status;

  // If the command-line switch is set, then it takes precedence over
  // the field trial group.
  if (!GetSecurityLevelAndHistogramValueForNonSecureFieldTrial(
          choice, displayed_sensitive_input_on_http, &level, &status)) {
    if (!GetSecurityLevelAndHistogramValueForNonSecureFieldTrial(
            group, displayed_sensitive_input_on_http, &level, &status)) {
      // If neither the command-line switch nor field trial group is set, then
      // nonsecure defaults to neutral.
      status = NEUTRAL;
      level = SecurityStateModel::NONE;
    }
  }

  UMA_HISTOGRAM_ENUMERATION(kEnumeration, status, LAST_STATUS);
  return level;
}

SecurityStateModel::SHA1DeprecationStatus GetSHA1DeprecationStatus(
    scoped_refptr<net::X509Certificate> cert,
    const SecurityStateModel::VisibleSecurityState& visible_security_state) {
  if (!cert ||
      !(visible_security_state.cert_status &
        net::CERT_STATUS_SHA1_SIGNATURE_PRESENT))
    return SecurityStateModel::NO_DEPRECATED_SHA1;

  // The internal representation of the dates for UI treatment of SHA-1.
  // See http://crbug.com/401365 for details.
  static const int64_t kJanuary2017 = INT64_C(13127702400000000);
  if (cert->valid_expiry() >= base::Time::FromInternalValue(kJanuary2017))
    return SecurityStateModel::DEPRECATED_SHA1_MAJOR;
  static const int64_t kJanuary2016 = INT64_C(13096080000000000);
  if (cert->valid_expiry() >= base::Time::FromInternalValue(kJanuary2016))
    return SecurityStateModel::DEPRECATED_SHA1_MINOR;

  return SecurityStateModel::NO_DEPRECATED_SHA1;
}

SecurityStateModel::ContentStatus GetContentStatus(bool displayed, bool ran) {
  if (ran && displayed)
    return SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN;
  if (ran)
    return SecurityStateModel::CONTENT_STATUS_RAN;
  if (displayed)
    return SecurityStateModel::CONTENT_STATUS_DISPLAYED;
  return SecurityStateModel::CONTENT_STATUS_NONE;
}

SecurityStateModel::SecurityLevel GetSecurityLevelForRequest(
    const SecurityStateModel::VisibleSecurityState& visible_security_state,
    SecurityStateModelClient* client,
    const scoped_refptr<net::X509Certificate>& cert,
    SecurityStateModel::SHA1DeprecationStatus sha1_status,
    SecurityStateModel::ContentStatus mixed_content_status,
    SecurityStateModel::ContentStatus content_with_cert_errors_status) {
  DCHECK(visible_security_state.connection_info_initialized ||
         visible_security_state.fails_malware_check);

  // Override the connection security information if the website failed the
  // browser's malware checks.
  if (visible_security_state.fails_malware_check)
    return SecurityStateModel::DANGEROUS;

  GURL url = visible_security_state.url;
  switch (visible_security_state.initial_security_level) {
    case SecurityStateModel::NONE:
    case SecurityStateModel::HTTP_SHOW_WARNING: {
      if (!client->IsOriginSecure(url) && url.IsStandard()) {
        return GetSecurityLevelForNonSecureFieldTrial(
            visible_security_state.displayed_password_field_on_http ||
            visible_security_state.displayed_credit_card_field_on_http);
      }
      return SecurityStateModel::NONE;
    }

    case SecurityStateModel::DANGEROUS:
      return SecurityStateModel::DANGEROUS;

    case SecurityStateModel::SECURITY_WARNING:
    case SecurityStateModel::SECURE_WITH_POLICY_INSTALLED_CERT:
      return visible_security_state.initial_security_level;

    case SecurityStateModel::SECURE:
    case SecurityStateModel::EV_SECURE: {
      // Major cert errors and active mixed content will generally be
      // downgraded by the embedder to DANGEROUS and handled above,
      // but downgrade here just in case.
      net::CertStatus cert_status = visible_security_state.cert_status;
      if (net::IsCertStatusError(cert_status) &&
          !net::IsCertStatusMinorError(cert_status)) {
        return SecurityStateModel::DANGEROUS;
      }
      if (mixed_content_status == SecurityStateModel::CONTENT_STATUS_RAN ||
          mixed_content_status ==
              SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN ||
          content_with_cert_errors_status ==
              SecurityStateModel::CONTENT_STATUS_RAN ||
          content_with_cert_errors_status ==
              SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN) {
        return SecurityStateModel::kRanInsecureContentLevel;
      }

      // Report if there is a policy cert first, before reporting any other
      // authenticated-but-with-errors cases. A policy cert is a strong
      // indicator of a MITM being present (the enterprise), while the
      // other authenticated-but-with-errors indicate something may
      // be wrong, or may be wrong in the future, but is unclear now.
      if (client->UsedPolicyInstalledCertificate())
        return SecurityStateModel::SECURE_WITH_POLICY_INSTALLED_CERT;

      if (sha1_status == SecurityStateModel::DEPRECATED_SHA1_MAJOR)
        return SecurityStateModel::DANGEROUS;
      if (sha1_status == SecurityStateModel::DEPRECATED_SHA1_MINOR)
        return SecurityStateModel::NONE;

      // Active mixed content is handled above.
      DCHECK_NE(SecurityStateModel::CONTENT_STATUS_RAN, mixed_content_status);
      DCHECK_NE(SecurityStateModel::CONTENT_STATUS_DISPLAYED_AND_RAN,
                mixed_content_status);

      if (mixed_content_status ==
              SecurityStateModel::CONTENT_STATUS_DISPLAYED ||
          content_with_cert_errors_status ==
              SecurityStateModel::CONTENT_STATUS_DISPLAYED) {
        return SecurityStateModel::kDisplayedInsecureContentLevel;
      }

      if (net::IsCertStatusError(cert_status)) {
        // Major cert errors are handled above.
        DCHECK(net::IsCertStatusMinorError(cert_status));
        return SecurityStateModel::NONE;
      }
      if (net::SSLConnectionStatusToVersion(
              visible_security_state.connection_status) ==
          net::SSL_CONNECTION_VERSION_SSL3) {
        // SSLv3 will be removed in the future.
        return SecurityStateModel::SECURITY_WARNING;
      }
      if ((cert_status & net::CERT_STATUS_IS_EV) && cert)
        return SecurityStateModel::EV_SECURE;
      return SecurityStateModel::SECURE;
    }
  }

  return SecurityStateModel::NONE;
}

void SecurityInfoForRequest(
    SecurityStateModelClient* client,
    const SecurityStateModel::VisibleSecurityState& visible_security_state,
    const scoped_refptr<net::X509Certificate>& cert,
    SecurityStateModel::SecurityInfo* security_info) {
  if (!visible_security_state.connection_info_initialized) {
    *security_info = SecurityStateModel::SecurityInfo();
    security_info->fails_malware_check =
        visible_security_state.fails_malware_check;
    if (security_info->fails_malware_check) {
      security_info->security_level = GetSecurityLevelForRequest(
          visible_security_state, client, cert,
          SecurityStateModel::UNKNOWN_SHA1,
          SecurityStateModel::CONTENT_STATUS_UNKNOWN,
          SecurityStateModel::CONTENT_STATUS_UNKNOWN);
    }
    return;
  }
  security_info->certificate = visible_security_state.certificate;
  security_info->sha1_deprecation_status =
      GetSHA1DeprecationStatus(cert, visible_security_state);
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

  security_info->fails_malware_check =
      visible_security_state.fails_malware_check;

  security_info->security_level = GetSecurityLevelForRequest(
      visible_security_state, client, cert,
      security_info->sha1_deprecation_status,
      security_info->mixed_content_status,
      security_info->content_with_cert_errors_status);
}

}  // namespace

const SecurityStateModel::SecurityLevel
    SecurityStateModel::kDisplayedInsecureContentLevel =
        SecurityStateModel::NONE;
const SecurityStateModel::SecurityLevel
    SecurityStateModel::kRanInsecureContentLevel =
        SecurityStateModel::DANGEROUS;

SecurityStateModel::SecurityInfo::SecurityInfo()
    : security_level(SecurityStateModel::NONE),
      fails_malware_check(false),
      sha1_deprecation_status(SecurityStateModel::NO_DEPRECATED_SHA1),
      mixed_content_status(SecurityStateModel::CONTENT_STATUS_NONE),
      content_with_cert_errors_status(SecurityStateModel::CONTENT_STATUS_NONE),
      scheme_is_cryptographic(false),
      cert_status(0),
      security_bits(-1),
      connection_status(0),
      key_exchange_group(0),
      obsolete_ssl_status(net::OBSOLETE_SSL_NONE),
      pkp_bypassed(false) {}

SecurityStateModel::SecurityInfo::~SecurityInfo() {}

SecurityStateModel::SecurityStateModel() {}

SecurityStateModel::~SecurityStateModel() {}

void SecurityStateModel::GetSecurityInfo(
    SecurityStateModel::SecurityInfo* result) const {
  scoped_refptr<net::X509Certificate> cert = nullptr;
  client_->RetrieveCert(&cert);

  VisibleSecurityState new_visible_state;
  client_->GetVisibleSecurityState(&new_visible_state);
  SecurityInfoForRequest(client_, new_visible_state, cert, result);
}

void SecurityStateModel::SetClient(SecurityStateModelClient* client) {
  client_ = client;
}

SecurityStateModel::VisibleSecurityState::VisibleSecurityState()
    : initial_security_level(SecurityStateModel::NONE),
      fails_malware_check(false),
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

SecurityStateModel::VisibleSecurityState::~VisibleSecurityState() {}

bool SecurityStateModel::VisibleSecurityState::operator==(
    const SecurityStateModel::VisibleSecurityState& other) const {
  return (url == other.url &&
          initial_security_level == other.initial_security_level &&
          fails_malware_check == other.fails_malware_check &&
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
