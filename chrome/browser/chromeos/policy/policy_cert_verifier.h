// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_VERIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_VERIFIER_H_

#include "base/memory/scoped_ptr.h"
#include "net/cert/cert_verifier.h"

namespace net {
class CertTrustAnchorProvider;
}

namespace policy {

// Wraps a MultiThreadedCertVerifier to make it use the additional trust anchors
// configured by the ONC user policy.
class PolicyCertVerifier : public net::CertVerifier {
 public:
  // |profile| is a handle to the Profile whose request context makes use of
  // this verified. This object can be created on the IO thread; the handle is
  // only used on the UI thread, if it's still valid.
  // |trust_anchor_provider| is used to retrieve the current list of trust
  // anchors.
  PolicyCertVerifier(void* profile,
                     net::CertTrustAnchorProvider* trust_anchor_provider);
  virtual ~PolicyCertVerifier();

  // CertVerifier implementation:
  // Note: |callback| can be null.
  virtual int Verify(net::X509Certificate* cert,
                     const std::string& hostname,
                     int flags,
                     net::CRLSet* crl_set,
                     net::CertVerifyResult* verify_result,
                     const net::CompletionCallback& callback,
                     RequestHandle* out_req,
                     const net::BoundNetLog& net_log) OVERRIDE;

  virtual void CancelRequest(RequestHandle req) OVERRIDE;

 private:
  void* profile_;
  scoped_ptr<CertVerifier> delegate_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_VERIFIER_H_
