// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/connection_security.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_error_info.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/ssl_status.h"
#include "net/base/net_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#endif

namespace {

connection_security::SecurityLevel GetSecurityLevelForNonSecureFieldTrial() {
  std::string choice =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kMarkNonSecureAs);
  std::string group = base::FieldTrialList::FindFullName("MarkNonSecureAs");

  // Do not change this enum. It is used in the histogram.
  enum MarkNonSecureStatus { NEUTRAL, DUBIOUS, NON_SECURE, LAST_STATUS };
  const char kEnumeration[] = "MarkNonSecureAs";

  connection_security::SecurityLevel level;
  MarkNonSecureStatus status;

  if (choice == switches::kMarkNonSecureAsNeutral) {
    status = NEUTRAL;
    level = connection_security::NONE;
  } else if (choice == switches::kMarkNonSecureAsNonSecure) {
    status = NON_SECURE;
    level = connection_security::SECURITY_ERROR;
  } else if (group == switches::kMarkNonSecureAsNeutral) {
    status = NEUTRAL;
    level = connection_security::NONE;
  } else if (group == switches::kMarkNonSecureAsNonSecure) {
    status = NON_SECURE;
    level = connection_security::SECURITY_ERROR;
  } else {
    status = NEUTRAL;
    level = connection_security::NONE;
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

connection_security::SHA1DeprecationStatus GetSHA1DeprecationStatus(
    scoped_refptr<net::X509Certificate> cert,
    const content::SSLStatus& ssl) {
  if (!cert || !(ssl.cert_status & net::CERT_STATUS_SHA1_SIGNATURE_PRESENT))
    return connection_security::NO_DEPRECATED_SHA1;

  // The internal representation of the dates for UI treatment of SHA-1.
  // See http://crbug.com/401365 for details.
  static const int64_t kJanuary2017 = INT64_C(13127702400000000);
  if (cert->valid_expiry() >= base::Time::FromInternalValue(kJanuary2017))
    return connection_security::DEPRECATED_SHA1_BROKEN;
  // kJanuary2016 needs to be kept in sync with
  // ToolbarModelAndroid::IsDeprecatedSHA1Present().
  static const int64_t kJanuary2016 = INT64_C(13096080000000000);
  if (cert->valid_expiry() >= base::Time::FromInternalValue(kJanuary2016))
    return connection_security::DEPRECATED_SHA1_WARNING;

  return connection_security::NO_DEPRECATED_SHA1;
}

connection_security::MixedContentStatus GetMixedContentStatus(
    const content::SSLStatus& ssl) {
  bool ran_insecure_content = false;
  bool displayed_insecure_content = false;
  if (ssl.content_status & content::SSLStatus::RAN_INSECURE_CONTENT)
    ran_insecure_content = true;
  if (ssl.content_status & content::SSLStatus::DISPLAYED_INSECURE_CONTENT)
    displayed_insecure_content = true;

  if (ran_insecure_content && displayed_insecure_content)
    return connection_security::RAN_AND_DISPLAYED_MIXED_CONTENT;
  if (ran_insecure_content)
    return connection_security::RAN_MIXED_CONTENT;
  if (displayed_insecure_content)
    return connection_security::DISPLAYED_MIXED_CONTENT;

  return connection_security::NO_MIXED_CONTENT;
}

}  // namespace

namespace connection_security {

SecurityLevel GetSecurityLevelForWebContents(
    const content::WebContents* web_contents) {
  if (!web_contents)
    return NONE;

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  if (!entry)
    return NONE;

  const content::SSLStatus& ssl = entry->GetSSL();
  switch (ssl.security_style) {
    case content::SECURITY_STYLE_UNKNOWN:
      return NONE;

    case content::SECURITY_STYLE_UNAUTHENTICATED: {
      const GURL& url = entry->GetURL();
      if (!content::IsOriginSecure(url))
        return GetSecurityLevelForNonSecureFieldTrial();
      return NONE;
    }

    case content::SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return SECURITY_ERROR;

    case content::SECURITY_STYLE_AUTHENTICATED: {
#if defined(OS_CHROMEOS)
      // Report if there is a policy cert first, before reporting any other
      // authenticated-but-with-errors cases. A policy cert is a strong
      // indicator of a MITM being present (the enterprise), while the
      // other authenticated-but-with-errors indicate something may
      // be wrong, or may be wrong in the future, but is unclear now.
      policy::PolicyCertService* service =
          policy::PolicyCertServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()));
      if (service && service->UsedPolicyCertificates())
        return SECURITY_POLICY_WARNING;
#endif

      scoped_refptr<net::X509Certificate> cert = GetCertForSSLStatus(ssl);
      SHA1DeprecationStatus sha1_status = GetSHA1DeprecationStatus(cert, ssl);
      if (sha1_status == DEPRECATED_SHA1_BROKEN)
        return SECURITY_ERROR;
      if (sha1_status == DEPRECATED_SHA1_WARNING)
        return NONE;

      MixedContentStatus mixed_content_status = GetMixedContentStatus(ssl);
      // Active mixed content is downgraded to the BROKEN style and
      // handled above.
      DCHECK_NE(RAN_MIXED_CONTENT, mixed_content_status);
      DCHECK_NE(RAN_AND_DISPLAYED_MIXED_CONTENT, mixed_content_status);
      // This should be kept in sync with
      // |kDisplayedInsecureContentStyle|. That is: the treatment
      // given to passive mixed content here should be expressed by
      // |kDisplayedInsecureContentStyle|, which is used to coordinate
      // the treatment of passive mixed content with other security UI
      // elements.
      if (mixed_content_status == DISPLAYED_MIXED_CONTENT)
        return NONE;

      if (net::IsCertStatusError(ssl.cert_status)) {
        DCHECK(net::IsCertStatusMinorError(ssl.cert_status));
        return NONE;
      }
      if (net::SSLConnectionStatusToVersion(ssl.connection_status) ==
          net::SSL_CONNECTION_VERSION_SSL3) {
        // SSLv3 will be removed in the future.
        return SECURITY_WARNING;
      }
      if ((ssl.cert_status & net::CERT_STATUS_IS_EV) && cert)
        return EV_SECURE;
      return SECURE;
    }

    default:
      NOTREACHED();
      return NONE;
  }
}

void GetSecurityInfoForWebContents(const content::WebContents* web_contents,
                                   SecurityInfo* security_info) {
  content::NavigationEntry* entry =
      web_contents ? web_contents->GetController().GetVisibleEntry() : nullptr;
  if (!entry) {
    security_info->security_style = content::SECURITY_STYLE_UNKNOWN;
    return;
  }

  security_info->scheme_is_cryptographic =
      entry->GetURL().SchemeIsCryptographic();

  SecurityLevel security_level = GetSecurityLevelForWebContents(web_contents);
  switch (security_level) {
    case SECURITY_WARNING:
    case NONE:
      security_info->security_style = content::SECURITY_STYLE_UNAUTHENTICATED;
      break;
    case EV_SECURE:
    case SECURE:
      security_info->security_style = content::SECURITY_STYLE_AUTHENTICATED;
      break;
    case SECURITY_POLICY_WARNING:
      security_info->security_style = content::SECURITY_STYLE_WARNING;
      break;
    case SECURITY_ERROR:
      security_info->security_style =
          content::SECURITY_STYLE_AUTHENTICATION_BROKEN;
      break;
  }

  const content::SSLStatus& ssl = entry->GetSSL();
  scoped_refptr<net::X509Certificate> cert = GetCertForSSLStatus(ssl);
  security_info->sha1_deprecation_status = GetSHA1DeprecationStatus(cert, ssl);
  security_info->mixed_content_status = GetMixedContentStatus(ssl);
  security_info->cert_status = ssl.cert_status;
}

}  // namespace connection_security
