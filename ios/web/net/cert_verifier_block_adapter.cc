// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/net/cert_verifier_block_adapter.h"

#include "base/mac/bind_objc_block.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/crl_set.h"
#include "net/cert/x509_certificate.h"

namespace net {

namespace {

// Resource manager which keeps CertVerifier::Request, CertVerifyResult and
// X509Certificate alive until verification is completed.
struct VerificationContext : public base::RefCounted<VerificationContext> {
  VerificationContext(scoped_refptr<net::X509Certificate> cert) : cert(cert) {
    result.cert_status = CERT_STATUS_INVALID;
  }
  // Verification request. Must be alive until verification is completed,
  // otherwise it will be cancelled.
  scoped_ptr<CertVerifier::Request> request;
  // The result of certificate verification.
  CertVerifyResult result;
  // Certificate being verificated.
  scoped_refptr<net::X509Certificate> cert;

  // Copies CertVerifyResult and wraps it into a scoped_ptr.
  scoped_ptr<CertVerifyResult> scoped_result() {
    scoped_ptr<CertVerifyResult> scoped_result(new CertVerifyResult());
    scoped_result->CopyFrom(result);
    return scoped_result.Pass();
  }

 private:
  VerificationContext() = delete;
  // Required by base::RefCounted.
  friend class base::RefCounted<VerificationContext>;
  ~VerificationContext() {}
};
}

CertVerifierBlockAdapter::CertVerifierBlockAdapter()
    : CertVerifierBlockAdapter(
          scoped_ptr<CertVerifier>(CertVerifier::CreateDefault())) {
}

CertVerifierBlockAdapter::CertVerifierBlockAdapter(
    scoped_ptr<CertVerifier> cert_verifier)
    : cert_verifier_(cert_verifier.Pass()) {
  DCHECK(cert_verifier_);
}

CertVerifierBlockAdapter::~CertVerifierBlockAdapter() {
}

CertVerifierBlockAdapter::Params::Params(scoped_refptr<X509Certificate> cert,
                                         const std::string& hostname)
    : cert(cert),
      hostname(hostname),
      flags(static_cast<CertVerifier::VerifyFlags>(0)) {
}

CertVerifierBlockAdapter::Params::~Params() {
}

void CertVerifierBlockAdapter::Verify(
    const Params& params,
    void (^completion_handler)(scoped_ptr<CertVerifyResult>, int)) {
  DCHECK(params.cert);
  DCHECK(params.hostname.size());

  scoped_refptr<VerificationContext> context(
      new VerificationContext(params.cert));
  CompletionCallback callback = base::BindBlock(^(int) {
    completion_handler(context->scoped_result(), 0);
  });
  int status = cert_verifier_->Verify(params.cert.get(), params.hostname,
                                      params.ocsp_response, params.flags,
                                      params.crl_set.get(), &(context->result),
                                      callback, &(context->request), net_log_);
  DCHECK(status != ERR_INVALID_ARGUMENT);

  if (status == ERR_IO_PENDING) {
    // Completion handler will be called from |callback| when verification
    // request is completed.
    return;
  }

  // Verification has either failed or result was retrieved from the cache.
  completion_handler(status ? nullptr : context->scoped_result(), status);
}

}  // net
