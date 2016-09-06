// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/security_style_util.h"

#include "url/gurl.h"

namespace content {

SecurityStyle GetSecurityStyleForResource(
    const GURL& url,
    bool has_certificate,
    net::CertStatus cert_status) {
  // An HTTPS response may not have a certificate for some reason.  When that
  // happens, use the unauthenticated (HTTP) rather than the authentication
  // broken security style so that we can detect this error condition.
  if (!url.SchemeIsCryptographic() || !has_certificate)
    return SECURITY_STYLE_UNAUTHENTICATED;

  // Minor errors don't lower the security style to
  // SECURITY_STYLE_AUTHENTICATION_BROKEN.
  if (net::IsCertStatusError(cert_status) &&
      !net::IsCertStatusMinorError(cert_status)) {
    return SECURITY_STYLE_AUTHENTICATION_BROKEN;
  }

  return SECURITY_STYLE_AUTHENTICATED;
}

}  // namespace content
