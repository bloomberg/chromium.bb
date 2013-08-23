// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_VERIFIER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_VERIFIER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/cert/cert_trust_anchor_provider.h"
#include "net/cert/cert_verifier.h"

namespace net {
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace policy {

// Wraps a MultiThreadedCertVerifier to make it use the additional trust anchors
// configured by the ONC user policy.
class PolicyCertVerifier : public net::CertVerifier,
                           public net::CertTrustAnchorProvider {
 public:
  // This object must be created on the UI thread. It's member functions and
  // destructor must be called on the IO thread. |profile| is a handle to the
  // Profile whose request context makes use of this Verifier. The handle is
  // only used on the UI thread, if it's still valid.
  explicit PolicyCertVerifier(void* profile);
  virtual ~PolicyCertVerifier();

  void InitializeOnIOThread();

  void SetTrustAnchors(const net::CertificateList& trust_anchors);

  // CertVerifier:
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

  // CertTrustAnchorProvider:
  virtual const net::CertificateList& GetAdditionalTrustAnchors() OVERRIDE;

 private:
  net::CertificateList trust_anchors_;
  void* profile_;
  scoped_ptr<CertVerifier> delegate_;

  DISALLOW_COPY_AND_ASSIGN(PolicyCertVerifier);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_POLICY_CERT_VERIFIER_H_
