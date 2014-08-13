// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SSL_HOST_STATE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SSL_HOST_STATE_DELEGATE_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "net/cert/x509_certificate.h"

namespace content {

// SSLHostStateDelegate may be implemented by the embedder to provide a storage
// strategy for certificate decisions.
class SSLHostStateDelegate {
 public:
  // Records that |cert| is not permitted to be used for |host| in the future,
  // for a specified |error| type.
  virtual void DenyCert(const std::string& host,
                        net::X509Certificate* cert,
                        net::CertStatus error) = 0;

  // Records that |cert| is permitted to be used for |host| in the future, for
  // a specified |error| type.
  virtual void AllowCert(const std::string&,
                         net::X509Certificate* cert,
                         net::CertStatus error) = 0;

  // Clear all allow/deny preferences.
  virtual void Clear() = 0;

  // Queries whether |cert| is allowed or denied for |host| and |error|.
  virtual net::CertPolicy::Judgment QueryPolicy(const std::string& host,
                                                net::X509Certificate* cert,
                                                net::CertStatus error) = 0;

  // Revoke all allow/deny preferences for |host|.
  virtual void RevokeAllowAndDenyPreferences(const std::string& host) = 0;

  // Returns true if any decisions has been recorded for |host|, otherwise
  // false.
  virtual bool HasAllowedOrDeniedCert(const std::string& host) = 0;

 protected:
  virtual ~SSLHostStateDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SSL_HOST_STATE_DELEGATE_H_
