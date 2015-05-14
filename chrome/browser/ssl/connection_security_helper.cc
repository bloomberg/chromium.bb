// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/connection_security_helper.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
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

ConnectionSecurityHelper::SecurityLevel
GetSecurityLevelForNonSecureFieldTrial() {
  std::string choice =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kMarkNonSecureAs);
  if (choice == switches::kMarkNonSecureAsNeutral)
    return ConnectionSecurityHelper::NONE;
  if (choice == switches::kMarkNonSecureAsDubious)
    return ConnectionSecurityHelper::SECURITY_WARNING;
  if (choice == switches::kMarkNonSecureAsNonSecure)
    return ConnectionSecurityHelper::SECURITY_ERROR;

  std::string group = base::FieldTrialList::FindFullName("MarkNonSecureAs");
  if (group == switches::kMarkNonSecureAsNeutral)
    return ConnectionSecurityHelper::NONE;
  if (group == switches::kMarkNonSecureAsDubious)
    return ConnectionSecurityHelper::SECURITY_WARNING;
  if (group == switches::kMarkNonSecureAsNonSecure)
    return ConnectionSecurityHelper::SECURITY_ERROR;

  return ConnectionSecurityHelper::NONE;
}

}  // namespace

ConnectionSecurityHelper::SecurityLevel
ConnectionSecurityHelper::GetSecurityLevelForWebContents(
    content::WebContents* web_contents) {
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
      policy::PolicyCertService* service =
          policy::PolicyCertServiceFactory::GetForProfile(
              Profile::FromBrowserContext(web_contents->GetBrowserContext()));
      if (service && service->UsedPolicyCertificates())
        return SECURITY_POLICY_WARNING;
#endif
      if (ssl.content_status & content::SSLStatus::DISPLAYED_INSECURE_CONTENT)
        return SECURITY_WARNING;
      scoped_refptr<net::X509Certificate> cert;
      if (content::CertStore::GetInstance()->RetrieveCert(ssl.cert_id, &cert) &&
          (ssl.cert_status & net::CERT_STATUS_SHA1_SIGNATURE_PRESENT)) {
        // The internal representation of the dates for UI treatment of SHA-1.
        // See http://crbug.com/401365 for details.
        static const int64_t kJanuary2017 = INT64_C(13127702400000000);
        // kJanuary2016 needs to be kept in sync with
        // ToolbarModelAndroid::IsDeprecatedSHA1Present().
        static const int64_t kJanuary2016 = INT64_C(13096080000000000);
        if (cert->valid_expiry() >=
            base::Time::FromInternalValue(kJanuary2017)) {
          return SECURITY_ERROR;
        }
        if (cert->valid_expiry() >=
            base::Time::FromInternalValue(kJanuary2016)) {
          return SECURITY_WARNING;
        }
      }
      if (net::IsCertStatusError(ssl.cert_status)) {
        DCHECK(net::IsCertStatusMinorError(ssl.cert_status));
        return SECURITY_WARNING;
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
