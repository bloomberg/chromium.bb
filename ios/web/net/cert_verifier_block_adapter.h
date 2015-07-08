// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_CERT_VERIFIER_BLOCK_ADAPTER_H_
#define IOS_WEB_NET_CERT_VERIFIER_BLOCK_ADAPTER_H_

#include "base/memory/scoped_ptr.h"
#include "net/cert/cert_verifier.h"
#include "net/log/net_log.h"

namespace net {

class CertVerifyResult;
class CRLSet;
class X509Certificate;

// Provides block-based interface for net::CertVerifier.
class CertVerifierBlockAdapter {
 public:
  CertVerifierBlockAdapter();
  // Constructs adapter with given |CertVerifier| which can not be null.
  CertVerifierBlockAdapter(scoped_ptr<CertVerifier> cert_verifier);

  // When the verifier is destroyed, all certificate verification requests are
  // canceled, and their completion handlers will not be called.
  ~CertVerifierBlockAdapter();

  // Encapsulates verification parms. |cert| and |hostname| are mandatory, the
  // other params are optional.
  struct Params {
    // Constructs Params from X509 cert and hostname, which are mandatory for
    // verification.
    Params(scoped_refptr<net::X509Certificate> cert,
           const std::string& hostname);
    ~Params();

    // Certificate to verify, can not be null.
    scoped_refptr<net::X509Certificate> cert;

    // Hostname as an SSL server, can not be empty.
    std::string hostname;

    // If non-empty, is a stapled OCSP response to use.
    std::string ocsp_response;

    // Bitwise OR of CertVerifier::VerifyFlags.
    CertVerifier::VerifyFlags flags;

    // An optional CRLSet structure which can be used to avoid revocation checks
    // over the network.
    scoped_refptr<CRLSet> crl_set;
  };

  // Type of verification completion block. On success CertVerifyResult is not
  // null and status is OK, otherwise CertVerifyResult is null and status is a
  // net error code.
  typedef void (^CompletionHandler)(scoped_ptr<CertVerifyResult>, int status);

  // Verifies certificate with given |params|. |completion_handler| must not be
  // null and call be called either syncronously (in the same runloop) or
  // asyncronously.
  void Verify(const Params& params, CompletionHandler completion_handler);

 private:
  // Underlying CertVerifier.
  scoped_ptr<CertVerifier> cert_verifier_;
  // Net Log required by CertVerifier.
  BoundNetLog net_log_;
};

}  // net

#endif  // IOS_WEB_NET_CERT_VERIFIER_BLOCK_ADAPTER_H_
