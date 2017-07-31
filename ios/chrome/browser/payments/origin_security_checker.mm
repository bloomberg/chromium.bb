// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/payments/origin_security_checker.h"

#import "ios/web/public/origin_util.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace payments {

// static
bool OriginSecurityChecker::IsOriginSecure(const GURL& url) {
  return url.is_valid() && web::IsOriginSecure(url);
}

// static
bool OriginSecurityChecker::IsSchemeCryptographic(const GURL& url) {
  return url.is_valid() && url.SchemeIsCryptographic();
}

// static
bool OriginSecurityChecker::IsOriginLocalhostOrFile(const GURL& url) {
  return url.is_valid() &&
         (net::IsLocalhost(url.HostNoBracketsPiece()) || url.SchemeIsFile());
}

// static
bool OriginSecurityChecker::IsSSLCertificateValid(
    const security_state::SecurityLevel security_level) {
  return security_level == security_state::SECURE ||
         security_level == security_state::EV_SECURE ||
         security_level == security_state::SECURE_WITH_POLICY_INSTALLED_CERT;
}

}  // namespace payments
