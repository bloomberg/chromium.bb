// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_ORIGIN_SECURITY_CHECKER_H_
#define IOS_CHROME_BROWSER_PAYMENTS_ORIGIN_SECURITY_CHECKER_H_

#include "base/macros.h"
#include "components/security_state/core/security_state.h"

class GURL;

namespace payments {

class OriginSecurityChecker {
 public:
  // Returns true for a valid |url| from a secure origin.
  static bool IsOriginSecure(const GURL& url);

  // Returns true for a valid |url| with a cryptographic scheme, e.g., HTTPS,
  // HTTPS-SO, WSS.
  static bool IsSchemeCryptographic(const GURL& url);

  // Returns true for a valid |url| with localhost or file:// scheme origin.
  static bool IsOriginLocalhostOrFile(const GURL& url);

  // Returns true if the page has a valid SSL certificate. Only EV_SECURE,
  // SECURE, and SECURE_WITH_POLICY_INSTALLED_CERT are considered valid for web
  // payments.
  static bool IsSSLCertificateValid(
      const security_state::SecurityLevel security_level);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(OriginSecurityChecker);
};

}  // namespace payments

#endif  // IOS_CHROME_BROWSER_PAYMENTS_ORIGIN_SECURITY_CHECKER_H_
