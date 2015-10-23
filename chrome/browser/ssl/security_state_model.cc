// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/security_state_model.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "net/ssl/ssl_connection_status_flags.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SecurityStateModel);

namespace {

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

scoped_refptr<net::X509Certificate> GetCertForSSLStatus(
    const content::SSLStatus& ssl) {
  scoped_refptr<net::X509Certificate> cert;
  return content::CertStore::GetInstance()->RetrieveCert(ssl.cert_id, &cert)
             ? cert
             : nullptr;
}

SecurityStateModel::SHA1DeprecationStatus GetSHA1DeprecationStatus(
    scoped_refptr<net::X509Certificate> cert,
    const content::SSLStatus& ssl) {
  if (!cert || !(ssl.cert_status & net::CERT_STATUS_SHA1_SIGNATURE_PRESENT))
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
    const content::SSLStatus& ssl) {
  bool ran_insecure_content = false;
  bool displayed_insecure_content = false;
  if (ssl.content_status & content::SSLStatus::RAN_INSECURE_CONTENT)
    ran_insecure_content = true;
  if (ssl.content_status & content::SSLStatus::DISPLAYED_INSECURE_CONTENT)
    displayed_insecure_content = true;

  if (ran_insecure_content && displayed_insecure_content)
    return SecurityStateModel::RAN_AND_DISPLAYED_MIXED_CONTENT;
  if (ran_insecure_content)
    return SecurityStateModel::RAN_MIXED_CONTENT;
  if (displayed_insecure_content)
    return SecurityStateModel::DISPLAYED_MIXED_CONTENT;

  return SecurityStateModel::NO_MIXED_CONTENT;
}

SecurityStateModel::SecurityLevel GetSecurityLevelForRequest(
    const GURL& url,
    const content::SSLStatus& ssl,
    Profile* profile,
    scoped_refptr<net::X509Certificate> cert,
    SecurityStateModel::SHA1DeprecationStatus sha1_status,
    SecurityStateModel::MixedContentStatus mixed_content_status) {
  switch (ssl.security_style) {
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
#if defined(OS_CHROMEOS)
      // Report if there is a policy cert first, before reporting any other
      // authenticated-but-with-errors cases. A policy cert is a strong
      // indicator of a MITM being present (the enterprise), while the
      // other authenticated-but-with-errors indicate something may
      // be wrong, or may be wrong in the future, but is unclear now.
      policy::PolicyCertService* service =
          policy::PolicyCertServiceFactory::GetForProfile(profile);
      if (service && service->UsedPolicyCertificates())
        return SecurityStateModel::SECURITY_POLICY_WARNING;
#endif

      if (sha1_status == SecurityStateModel::DEPRECATED_SHA1_MAJOR)
        return SecurityStateModel::SECURITY_ERROR;
      if (sha1_status == SecurityStateModel::DEPRECATED_SHA1_MINOR)
        return SecurityStateModel::NONE;

      // Active mixed content is downgraded to the BROKEN style and
      // handled above.
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

      if (net::IsCertStatusError(ssl.cert_status)) {
        DCHECK(net::IsCertStatusMinorError(ssl.cert_status));
        return SecurityStateModel::NONE;
      }
      if (net::SSLConnectionStatusToVersion(ssl.connection_status) ==
          net::SSL_CONNECTION_VERSION_SSL3) {
        // SSLv3 will be removed in the future.
        return SecurityStateModel::SECURITY_WARNING;
      }
      if ((ssl.cert_status & net::CERT_STATUS_IS_EV) && cert)
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
      connection_status(0) {}

SecurityStateModel::SecurityInfo::~SecurityInfo() {}

SecurityStateModel::~SecurityStateModel() {}

const SecurityStateModel::SecurityInfo& SecurityStateModel::GetSecurityInfo()
    const {
  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (!entry) {
    security_info_ = SecurityInfo();
    visible_url_ = GURL();
    visible_ssl_status_ = content::SSLStatus();
    return security_info_;
  }

  if (entry->GetURL() == visible_url_ &&
      entry->GetSSL().Equals(visible_ssl_status_)) {
    // A cert must be present in the CertStore in order for the site to
    // be considered EV_SECURE, and the cert might have been removed
    // since the security level was last computed.
    if (security_info_.security_level == EV_SECURE &&
        !GetCertForSSLStatus(visible_ssl_status_)) {
      security_info_.security_level = SECURE;
    }
    return security_info_;
  }

  SecurityInfoForRequest(
      entry->GetURL(), entry->GetSSL(),
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()),
      &security_info_);
  visible_url_ = entry->GetURL();
  visible_ssl_status_ = entry->GetSSL();
  return security_info_;
}

// static
void SecurityStateModel::SecurityInfoForRequest(const GURL& url,
                                                const content::SSLStatus& ssl,
                                                Profile* profile,
                                                SecurityInfo* security_info) {
  scoped_refptr<net::X509Certificate> cert = GetCertForSSLStatus(ssl);
  security_info->cert_id = ssl.cert_id;
  security_info->sha1_deprecation_status = GetSHA1DeprecationStatus(cert, ssl);
  security_info->mixed_content_status = GetMixedContentStatus(ssl);
  security_info->security_bits = ssl.security_bits;
  security_info->connection_status = ssl.connection_status;
  security_info->cert_status = ssl.cert_status;
  security_info->scheme_is_cryptographic = url.SchemeIsCryptographic();

  security_info->sct_verify_statuses.clear();
  for (const auto& sct : ssl.signed_certificate_timestamp_ids) {
    security_info->sct_verify_statuses.push_back(sct.status);
  }

  security_info->security_level = GetSecurityLevelForRequest(
      url, ssl, profile, cert, security_info->sha1_deprecation_status,
      security_info->mixed_content_status);
}

SecurityStateModel::SecurityStateModel(content::WebContents* web_contents)
    : web_contents_(web_contents) {}
