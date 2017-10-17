// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/security_state.h"

#include <stdint.h>
#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "components/security_state/core/switches.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"

namespace security_state {

namespace {

// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum MarkHttpStatus {
  NEUTRAL = 0,  // Deprecated
  NON_SECURE = 1,
  HTTP_SHOW_WARNING_ON_SENSITIVE_FIELDS = 2,  // Deprecated as of Chrome 64
  NON_SECURE_AFTER_EDITING = 3,
  NON_SECURE_WHILE_INCOGNITO = 4,
  NON_SECURE_WHILE_INCOGNITO_OR_EDITING = 5,
  LAST_STATUS
};

// If |switch_or_field_trial_group| corresponds to a valid
// MarkHttpAs setting, sets |*level| and |*histogram_status| to the
// appropriate values and returns true. Otherwise, returns false.
bool GetSecurityLevelAndHistogramValueForNonSecureFieldTrial(
    std::string switch_or_field_trial_group,
    bool is_incognito,
    const InsecureInputEventData& input_events,
    SecurityLevel* level,
    MarkHttpStatus* mark_http_as) {
  if (switch_or_field_trial_group ==
      switches::kMarkHttpAsNonSecureWhileIncognito) {
    *mark_http_as = NON_SECURE_WHILE_INCOGNITO;
    *level = (is_incognito || input_events.password_field_shown ||
              input_events.credit_card_field_edited)
                 ? security_state::HTTP_SHOW_WARNING
                 : NONE;
    return true;
  }
  if (switch_or_field_trial_group ==
      switches::kMarkHttpAsNonSecureWhileIncognitoOrEditing) {
    *mark_http_as = NON_SECURE_WHILE_INCOGNITO_OR_EDITING;
    *level = (is_incognito || input_events.insecure_field_edited ||
              input_events.password_field_shown ||
              input_events.credit_card_field_edited)
                 ? security_state::HTTP_SHOW_WARNING
                 : NONE;
    return true;
  }
  if (switch_or_field_trial_group ==
      switches::kMarkHttpAsNonSecureAfterEditing) {
    *mark_http_as = NON_SECURE_AFTER_EDITING;
    *level = (input_events.insecure_field_edited ||
              input_events.password_field_shown ||
              input_events.credit_card_field_edited)
                 ? security_state::HTTP_SHOW_WARNING
                 : NONE;
    return true;
  }
  if (switch_or_field_trial_group == switches::kMarkHttpAsDangerous) {
    *mark_http_as = NON_SECURE;
    *level = DANGEROUS;
    return true;
  }

  return false;
}

SecurityLevel GetSecurityLevelForNonSecureFieldTrial(
    bool is_incognito,
    const InsecureInputEventData& input_events,
    MarkHttpStatus* mark_http_as) {
  std::string choice =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kMarkHttpAs);
  std::string group = base::FieldTrialList::FindFullName("MarkNonSecureAs");

  const char kEnumeration[] = "SSL.MarkHttpAsStatus";

  SecurityLevel level = NONE;

  // If the command-line switch is set, then it takes precedence over
  // the field trial group.
  if (!GetSecurityLevelAndHistogramValueForNonSecureFieldTrial(
          choice, is_incognito, input_events, &level, mark_http_as)) {
    if (!GetSecurityLevelAndHistogramValueForNonSecureFieldTrial(
            group, is_incognito, input_events, &level, mark_http_as)) {
      // No command-line switch or field trial is in effect.
      // Default to warning on incognito or editing or sensitive form fields.
      *mark_http_as = NON_SECURE_WHILE_INCOGNITO_OR_EDITING;
      level = (is_incognito || input_events.insecure_field_edited ||
               input_events.password_field_shown ||
               input_events.credit_card_field_edited)
                  ? security_state::HTTP_SHOW_WARNING
                  : NONE;
    }
  }

  UMA_HISTOGRAM_ENUMERATION(kEnumeration, *mark_http_as, LAST_STATUS);
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
    ContentStatus content_with_cert_errors_status,
    MarkHttpStatus* mark_http_as) {
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
  // Likewise, ftp: URLs are always non-secure, and are uncommon enough that
  // we can treat them as such without significant user impact.
  //
  // Display a "Not secure" badge for all these URLs, regardless of whether
  // they show a password or credit card field.
  if (url.SchemeIs(url::kDataScheme) || url.SchemeIs(url::kFtpScheme))
    return SecurityLevel::HTTP_SHOW_WARNING;

  // Choose the appropriate security level for requests to HTTP and remaining
  // pseudo URLs (blob:, filesystem:). filesystem: is a standard scheme so does
  // not need to be explicitly listed here.
  // TODO(meacer): Remove special case for blob (crbug.com/684751).
  if (!is_cryptographic_with_certificate) {
    if (!visible_security_state.is_error_page &&
        !is_origin_secure_callback.Run(url) &&
        (url.IsStandard() || url.SchemeIs(url::kBlobScheme))) {
      return GetSecurityLevelForNonSecureFieldTrial(
          visible_security_state.is_incognito,
          visible_security_state.insecure_input_events, mark_http_as);
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

  if (visible_security_state.contained_mixed_form ||
      mixed_content_status == CONTENT_STATUS_DISPLAYED ||
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
  // |mark_http_as| will be updated to the value specified by the
  // field trial or command-line in |GetSecurityLevelForNonSecureFieldTrial|
  // if that function is reached.
  MarkHttpStatus mark_http_as = NON_SECURE_WHILE_INCOGNITO_OR_EDITING;

  if (!visible_security_state.connection_info_initialized) {
    *security_info = SecurityInfo();
    security_info->malicious_content_status =
        visible_security_state.malicious_content_status;
    if (security_info->malicious_content_status !=
        MALICIOUS_CONTENT_STATUS_NONE) {
      security_info->security_level = GetSecurityLevelForRequest(
          visible_security_state, used_policy_installed_certificate,
          is_origin_secure_callback, false, CONTENT_STATUS_UNKNOWN,
          CONTENT_STATUS_UNKNOWN, &mark_http_as);
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

  security_info->malicious_content_status =
      visible_security_state.malicious_content_status;

  security_info->cert_missing_subject_alt_name =
      visible_security_state.certificate &&
      !visible_security_state.certificate->GetSubjectAltName(nullptr, nullptr);

  security_info->contained_mixed_form =
      visible_security_state.contained_mixed_form;

  security_info->security_level = GetSecurityLevelForRequest(
      visible_security_state, used_policy_installed_certificate,
      is_origin_secure_callback, security_info->sha1_in_chain,
      security_info->mixed_content_status,
      security_info->content_with_cert_errors_status, &mark_http_as);

  security_info->incognito_downgraded_security_level =
      (visible_security_state.is_incognito &&
       !visible_security_state.is_error_page &&
       security_info->security_level == HTTP_SHOW_WARNING &&
       (mark_http_as == NON_SECURE_WHILE_INCOGNITO ||
        mark_http_as == NON_SECURE_WHILE_INCOGNITO_OR_EDITING));

  security_info->field_edit_downgraded_security_level =
      (security_info->security_level == HTTP_SHOW_WARNING &&
       visible_security_state.insecure_input_events.insecure_field_edited &&
       (mark_http_as == NON_SECURE_AFTER_EDITING ||
        mark_http_as == NON_SECURE_WHILE_INCOGNITO_OR_EDITING));

  security_info->insecure_input_events =
      visible_security_state.insecure_input_events;
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
      contained_mixed_form(false),
      cert_missing_subject_alt_name(false),
      incognito_downgraded_security_level(false),
      field_edit_downgraded_security_level(false) {}

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
      contained_mixed_form(false),
      ran_mixed_content(false),
      displayed_content_with_cert_errors(false),
      ran_content_with_cert_errors(false),
      pkp_bypassed(false),
      is_incognito(false),
      is_error_page(false) {}

VisibleSecurityState::~VisibleSecurityState() {}

bool IsSchemeCryptographic(const GURL& url) {
  return url.is_valid() && url.SchemeIsCryptographic();
}

bool IsOriginLocalhostOrFile(const GURL& url) {
  return url.is_valid() &&
         (net::IsLocalhost(url.HostNoBracketsPiece()) || url.SchemeIsFile());
}

bool IsSslCertificateValid(security_state::SecurityLevel security_level) {
  return security_level == security_state::SECURE ||
         security_level == security_state::EV_SECURE ||
         security_level == security_state::SECURE_WITH_POLICY_INSTALLED_CERT;
}

}  // namespace security_state
