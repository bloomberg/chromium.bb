// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/policy/policy_auditor.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "jni/PolicyAuditor_jni.h"
#include "net/cert/cert_status_flags.h"

int GetCertificateFailure(JNIEnv* env,
                          const JavaParamRef<jclass>& obj,
                          const JavaParamRef<jobject>& java_web_contents) {
  // This function is similar to
  // ToolbarModelImpl::GetSecurityLevelForWebContents, but has a custom mapping
  // for policy auditing
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.policy
  // GENERATED_JAVA_PREFIX_TO_STRIP: CERTIFICATE_FAIL_
  enum CertificateFailure {
    NONE = 0,
    CERTIFICATE_FAIL_UNSPECIFIED = 1,
    CERTIFICATE_FAIL_UNTRUSTED = 2,
    CERTIFICATE_FAIL_REVOKED = 3,
    CERTIFICATE_FAIL_NOT_YET_VALID = 4,
    CERTIFICATE_FAIL_EXPIRED = 5,
    CERTIFICATE_FAIL_UNABLE_TO_CHECK_REVOCATION_STATUS = 6,
  };

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  if (!entry)
    return NONE;

  const content::SSLStatus& ssl = entry->GetSSL();
  switch (ssl.security_style) {
    case content::SECURITY_STYLE_WARNING:
    case content::SECURITY_STYLE_UNKNOWN:
    case content::SECURITY_STYLE_UNAUTHENTICATED:
      return NONE;

    case content::SECURITY_STYLE_AUTHENTICATION_BROKEN:
    case content::SECURITY_STYLE_AUTHENTICATED: {
      if (net::IsCertStatusError(ssl.cert_status)) {
        if (ssl.cert_status & net::CERT_STATUS_AUTHORITY_INVALID)
          return CERTIFICATE_FAIL_UNTRUSTED;
        if (ssl.cert_status & net::CERT_STATUS_REVOKED)
          return CERTIFICATE_FAIL_REVOKED;
        // No mapping for CERTIFICATE_FAIL_NOT_YET_VALID.
        if (ssl.cert_status & net::CERT_STATUS_DATE_INVALID)
          return CERTIFICATE_FAIL_EXPIRED;
        if (ssl.cert_status & net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION)
          return CERTIFICATE_FAIL_UNABLE_TO_CHECK_REVOCATION_STATUS;
        return CERTIFICATE_FAIL_UNSPECIFIED;
      }
      if (ssl.content_status &
             content::SSLStatus::DISPLAYED_INSECURE_CONTENT) {
        return CERTIFICATE_FAIL_UNSPECIFIED;
      }
    }
  }
  return NONE;
}

bool RegisterPolicyAuditor(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
