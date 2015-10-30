// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CERT_VERIFIER_BLOCK_ADAPTER_H_
#define IOS_WEB_NET_CERT_VERIFIER_BLOCK_ADAPTER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"

namespace net {
class CRLSet;
class NetLog;
class X509Certificate;
}  // namespace net

namespace web {

// Provides block-based interface for |net::CertVerifier|. This class must be
// created and used on the same thread where the |net::CertVerifier| was
// created.
class CertVerifierBlockAdapter {
 public:
  // Constructs adapter with given |CertVerifier| and |NetLog|, both can not be
  // null. CertVerifierBlockAdapter does NOT take ownership of |cert_verifier|
  // and |net_log|.
  CertVerifierBlockAdapter(net::CertVerifier* cert_verifier,
                           net::NetLog* net_log);

  // When the verifier is destroyed, certificate verification requests are not
  // canceled, and their completion handlers are guaranteed to be called.
  ~CertVerifierBlockAdapter();

  // Encapsulates verification params. |cert| and |hostname| are mandatory, the
  // other params are optional. If either of mandatory arguments is null or
  // empty then verification |CompletionHandler| will be called with
  // ERR_INVALID_ARGUMENT |error|.
  struct Params {
    // Constructs Params from X509 cert and hostname, which are mandatory for
    // verification.
    Params(const scoped_refptr<net::X509Certificate>& cert,
           const std::string& hostname);
    ~Params();

    // Certificate to verify, can not be null.
    scoped_refptr<net::X509Certificate> cert;

    // Hostname as an SSL server, can not be empty.
    std::string hostname;

    // If non-empty, is a stapled OCSP response to use.
    std::string ocsp_response;

    // Bitwise OR of |net::CertVerifier::VerifyFlags|.
    int flags;

    // An optional |net::CRLSet| structure which can be used to avoid revocation
    // checks over the network.
    scoped_refptr<net::CRLSet> crl_set;
  };

  // Type of verification completion block. If cert is successfully validated
  // |error| is OK, otherwise |error| is a net error code.
  typedef void (^CompletionHandler)(net::CertVerifyResult result, int error);

  // Verifies certificate with given |params|. |completion_handler| must not be
  // null and can be called either synchronously (in the same runloop) or
  // asynchronously.
  // Note: |completion_handler| is guaranteed to be called, even if the instance
  // |Verify()| was called on is destroyed.
  void Verify(const Params& params, CompletionHandler completion_handler);

 private:
  // Underlying unowned CertVerifier.
  net::CertVerifier* cert_verifier_;
  // Unowned NetLog required by CertVerifier.
  net::NetLog* net_log_;
  // CertVerifierBlockAdapter should be used on the same thread where it was
  // created.
  base::ThreadChecker thread_checker_;
};

}  // namespace web

#endif  // IOS_WEB_NET_CERT_VERIFIER_BLOCK_ADAPTER_H_
