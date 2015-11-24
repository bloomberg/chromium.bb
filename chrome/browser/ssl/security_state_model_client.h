// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SECURITY_STATE_MODEL_CLIENT_H_
#define CHROME_BROWSER_SSL_SECURITY_STATE_MODEL_CLIENT_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace net {
class X509Certificate;
}  // namespace net

// Provides embedder-specific information that a SecurityStateModel
// needs to determine the page's security status.
class SecurityStateModelClient {
 public:
  SecurityStateModelClient() {}
  virtual ~SecurityStateModelClient() {}

  // Returns the certificate used to load the page or request.
  virtual bool RetrieveCert(scoped_refptr<net::X509Certificate>* cert) = 0;

  // Returns true if the page or request is known to be loaded with a
  // certificate installed by the system administrator.
  virtual bool UsedPolicyInstalledCertificate() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityStateModelClient);
};

#endif  // CHROME_BROWSER_SSL_SECURITY_STATE_MODEL_CLIENT_H_
