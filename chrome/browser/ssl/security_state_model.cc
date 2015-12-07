// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_model.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/ssl/security_state_model_client.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/origin_util.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"

namespace {

// TODO(estark): move this to SecurityStateModelClient as this is
// embedder-specific logic. https://crbug.com/515071
SecurityStateModel::SecurityLevel GetSecurityLevelForNonSecureFieldTrial() {
  std::string choice =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kMarkNonSecureAs);
  std::string group = base::FieldTrialList::FindFullName("MarkNonSecureAs");

  // Do not change this enum. It is used in the histogram.
  enum MarkNonSecureStatus { NEUTRAL, DUBIOUS, NON_SECURE, LAST_STATUS };
  const char kEnumeration[] = "MarkNonSecureAs";

  SecurityStateModel::SecurityLevel level = SecurityStateModel::NONE;
  MarkNonSecureStatus status;

  if (choice == switches::kMarkNonSecureAsNeutral) {
    status = NEUTRAL;
    level = SecurityStateModel::NONE;
  } else if (choice == switches::kMarkNonSecureAsNonSecure) {
    status = NON_SECURE;
    level = SecurityStateModel::SECURITY_ERROR;
  } else if (group == switches::kMarkNonSecureAsNeutral) {
    status = NEUTRAL;
    level = SecurityStateModel::NONE;
  } else if (group == switches::kMarkNonSecureAsNonSecure) {
    status = NON_SECURE;
    level = SecurityStateModel::SECURITY_ERROR;
  } else {
    status = NEUTRAL;
    level = SecurityStateModel::NONE;
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

SecurityStateModel::MixedContentStatus GetMixedContentStatus(
    const SecurityStateModel::VisibleSecurityState& visible_security_state) {
  bool ran_insecure_content = visible_security_state.ran_mixed_content;
  bool displayed_insecure_content =
      visible_security_state.displayed_mixed_content;
  if (ran_insecure_content && displayed_insecure_content)
    return SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT;
  if (ran_insecure_content)
    return SecurityStateModel::RAN_MIXED_CONTENT;
  if (displayed_insecure_content)
    return SecurityStateModel::DISPLAYED_MIXED_CONTENT;

  return SecurityStateModel::NO_MIXED_CONTENT;
}

SecurityStateModel::SecurityLevel GetSecurityLevelForRequest(
    const SecurityStateModel::VisibleSecurityState& visible_security_state,
    SecurityStateModelClient* client,
    const scoped_refptr<net::X509Certificate>& cert,
    SecurityStateModel::SHA1DeprecationStatus sha1_status,
    SecurityStateModel::MixedContentStatus mixed_content_status) {
  GURL url = visible_security_state.url;
  switch (visible_security_state.initial_security_style) {
    case content::SECURITY_STYLE_UNKNOWN:
      return SecurityStateModel::NONE;

    case content::SECURITY_STYLE_UNAUTHENTICATED: {
      if (!content::IsOriginSecure(url) && url.IsStandard())
        return GetSecurityLevelForNonSecureFieldTrial();
      return SecurityStateModel::NONE;
    }

    case content::SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return SecurityStateModel::SECURITY_ERROR;

    case content::SECURITY_STYLE_WARNING:
      NOTREACHED();
      return SecurityStateModel::SECURITY_WARNING;

    case content::SECURITY_STYLE_AUTHENTICATED: {
      // Major cert errors and active mixed content will generally be
      // downgraded by the embedder to
      // SECURITY_STYLE_AUTHENTICATION_BROKEN and handled above, but
      // downgrade to SECURITY_ERROR here just in case.
      net::CertStatus cert_status = visible_security_state.cert_status;
      if (net::IsCertStatusError(cert_status) &&
          !net::IsCertStatusMinorError(cert_status)) {
        return SecurityStateModel::SECURITY_ERROR;
      }
      if (mixed_content_status == SecurityStateModel::RAN_MIXED_CONTENT ||
          mixed_content_status ==
              SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT) {
        return SecurityStateModel::SECURITY_ERROR;
      }

      // Report if there is a policy cert first, before reporting any other
      // authenticated-but-with-errors cases. A policy cert is a strong
      // indicator of a MITM being present (the enterprise), while the
      // other authenticated-but-with-errors indicate something may
      // be wrong, or may be wrong in the future, but is unclear now.
      if (client->UsedPolicyInstalledCertificate())
        return SecurityStateModel::SECURITY_POLICY_WARNING;

      if (sha1_status == SecurityStateModel::DEPRECATED_SHA1_MAJOR)
        return SecurityStateModel::SECURITY_ERROR;
      if (sha1_status == SecurityStateModel::DEPRECATED_SHA1_MINOR)
        return SecurityStateModel::NONE;

      // Active mixed content is handled above.
      DCHECK_NE(SecurityStateModel::RAN_MIXED_CONTENT, mixed_content_status);
      DCHECK_NE(SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT,
                mixed_content_status);
      // This should be kept in sync with
      // |kDisplayedInsecureContentStyle|. That is: the treatment
      // given to passive mixed content here should be expressed by
      // |kDisplayedInsecureContentStyle|, which is used to coordinate
      // the treatment of passive mixed content with other security UI
      // elements outside of //chrome.
      if (mixed_content_status == SecurityStateModel::DISPLAYED_MIXED_CONTENT)
        return SecurityStateModel::NONE;

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

}  // namespace

const content::SecurityStyle
    SecurityStateModel::kDisplayedInsecureContentStyle =
        content::SECURITY_STYLE_UNAUTHENTICATED;
const content::SecurityStyle SecurityStateModel::kRanInsecureContentStyle =
    content::SECURITY_STYLE_AUTHENTICATION_BROKEN;

SecurityStateModel::SecurityInfo::SecurityInfo()
    : security_level(SecurityStateModel::NONE),
      sha1_deprecation_status(SecurityStateModel::NO_DEPRECATED_SHA1),
      mixed_content_status(SecurityStateModel::NO_MIXED_CONTENT),
      scheme_is_cryptographic(false),
      cert_status(0),
      cert_id(0),
      security_bits(-1),
      connection_status(0),
      is_secure_protocol_and_ciphersuite(false) {}

SecurityStateModel::SecurityInfo::~SecurityInfo() {}

SecurityStateModel::SecurityStateModel() {}

SecurityStateModel::~SecurityStateModel() {}

const SecurityStateModel::SecurityInfo& SecurityStateModel::GetSecurityInfo()
    const {
  scoped_refptr<net::X509Certificate> cert = nullptr;
  client_->RetrieveCert(&cert);

  // Check if the cached |security_info_| must be recomputed.
  VisibleSecurityState new_visible_state;
  client_->GetVisibleSecurityState(&new_visible_state);
  bool visible_security_state_changed =
      !(visible_security_state_ == new_visible_state);
  if (!visible_security_state_changed) {
    // A cert must be present in order for the site to be considered
    // EV_SECURE, and the cert might have been removed since the
    // security level was last computed.
    if (security_info_.security_level == EV_SECURE && !cert) {
      security_info_.security_level = SECURE;
    }
    return security_info_;
  }

  visible_security_state_ = new_visible_state;
  SecurityInfoForRequest(client_, visible_security_state_, cert,
                         &security_info_);
  return security_info_;
}

void SecurityStateModel::SetClient(SecurityStateModelClient* client) {
  client_ = client;
}

// static
void SecurityStateModel::SecurityInfoForRequest(
    SecurityStateModelClient* client,
    const VisibleSecurityState& visible_security_state,
    const scoped_refptr<net::X509Certificate>& cert,
    SecurityInfo* security_info) {
  security_info->cert_id = visible_security_state.cert_id;
  security_info->sha1_deprecation_status =
      GetSHA1DeprecationStatus(cert, visible_security_state);
  security_info->mixed_content_status =
      GetMixedContentStatus(visible_security_state);
  security_info->security_bits = visible_security_state.security_bits;
  security_info->connection_status = visible_security_state.connection_status;
  security_info->cert_status = visible_security_state.cert_status;
  security_info->scheme_is_cryptographic =
      visible_security_state.url.SchemeIsCryptographic();
  security_info->is_secure_protocol_and_ciphersuite =
      (net::SSLConnectionStatusToVersion(security_info->connection_status) >=
           net::SSL_CONNECTION_VERSION_TLS1_2 &&
       net::IsSecureTLSCipherSuite(net::SSLConnectionStatusToCipherSuite(
           security_info->connection_status)));

  security_info->sct_verify_statuses =
      visible_security_state.sct_verify_statuses;

  security_info->security_level =
      GetSecurityLevelForRequest(visible_security_state, client, cert,
                                 security_info->sha1_deprecation_status,
                                 security_info->mixed_content_status);
}

SecurityStateModel::VisibleSecurityState::VisibleSecurityState()
    : initial_security_style(content::SECURITY_STYLE_UNKNOWN),
      cert_id(0),
      cert_status(0),
      connection_status(0),
      security_bits(-1),
      displayed_mixed_content(false),
      ran_mixed_content(false) {}

SecurityStateModel::VisibleSecurityState::~VisibleSecurityState() {}

bool SecurityStateModel::VisibleSecurityState::operator==(
    const SecurityStateModel::VisibleSecurityState& other) const {
  return (url == other.url &&
          initial_security_style == other.initial_security_style &&
          cert_id == other.cert_id && cert_status == other.cert_status &&
          connection_status == other.connection_status &&
          security_bits == other.security_bits &&
          sct_verify_statuses == other.sct_verify_statuses &&
          displayed_mixed_content == other.displayed_mixed_content &&
          ran_mixed_content == other.ran_mixed_content);
}
