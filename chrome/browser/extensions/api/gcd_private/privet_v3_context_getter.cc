// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/gcd_private/privet_v3_context_getter.h"

#include "base/command_line.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/x509_certificate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"

namespace extensions {

// Class verifies certificate by its fingerprint received using different
// channel. It's the only know information about device with self-signed
// certificate.
class FingerprintVerifier : public net::CertVerifier {
 public:
  explicit FingerprintVerifier(
      const net::SHA256HashValue& certificate_fingerprint)
      : certificate_fingerprint_(certificate_fingerprint) {}

  int Verify(net::X509Certificate* cert,
             const std::string& hostname,
             const std::string& ocsp_response,
             int flags,
             net::CRLSet* crl_set,
             net::CertVerifyResult* verify_result,
             const net::CompletionCallback& callback,
             scoped_ptr<Request>* out_req,
             const net::BoundNetLog& net_log) override {
    // Mark certificate as invalid as we didn't check it.
    verify_result->Reset();
    verify_result->verified_cert = cert;
    verify_result->cert_status = net::CERT_STATUS_INVALID;

    auto fingerprint =
        net::X509Certificate::CalculateFingerprint256(cert->os_cert_handle());

    return certificate_fingerprint_.Equals(fingerprint) ? net::OK
                                                        : net::ERR_CERT_INVALID;
  }

 private:
  net::SHA256HashValue certificate_fingerprint_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintVerifier);
};

PrivetV3ContextGetter::PrivetV3ContextGetter(
    const scoped_refptr<base::SingleThreadTaskRunner>& net_task_runner,
    const net::SHA256HashValue& certificate_fingerprint)
    : verifier_(new FingerprintVerifier(certificate_fingerprint)),
      net_task_runner_(net_task_runner) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePrivetV3));
}

net::URLRequestContext* PrivetV3ContextGetter::GetURLRequestContext() {
  DCHECK(net_task_runner_->BelongsToCurrentThread());
  if (!context_) {
    net::URLRequestContextBuilder builder;
    builder.set_proxy_service(net::ProxyService::CreateDirect());
    builder.SetSpdyAndQuicEnabled(false, false);
    builder.DisableHttpCache();
    builder.SetCertVerifier(verifier_.Pass());
    builder.set_user_agent(::GetUserAgent());
    context_ = builder.Build();
  }
  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
PrivetV3ContextGetter::GetNetworkTaskRunner() const {
  return net_task_runner_;
}

PrivetV3ContextGetter::~PrivetV3ContextGetter() {
  DCHECK(net_task_runner_->BelongsToCurrentThread());
}

}  // namespace extensions
