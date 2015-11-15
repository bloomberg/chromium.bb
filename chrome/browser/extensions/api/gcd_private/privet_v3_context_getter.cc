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
class PrivetV3ContextGetter::CertVerifier : public net::CertVerifier {
 public:
  CertVerifier() {}

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

    auto it = fingerprints_.find(hostname);
    if (it == fingerprints_.end())
      return net::ERR_CERT_INVALID;

    auto fingerprint =
        net::X509Certificate::CalculateFingerprint256(cert->os_cert_handle());
    return it->second.Equals(fingerprint) ? net::OK : net::ERR_CERT_INVALID;
  }

  void AddPairedHost(const std::string& host,
                     const net::SHA256HashValue& certificate_fingerprint) {
    fingerprints_[host] = certificate_fingerprint;
  }

 private:
  std::map<std::string, net::SHA256HashValue> fingerprints_;

  DISALLOW_COPY_AND_ASSIGN(CertVerifier);
};

PrivetV3ContextGetter::PrivetV3ContextGetter(
    const scoped_refptr<base::SingleThreadTaskRunner>& net_task_runner)
    : net_task_runner_(net_task_runner), weak_ptr_factory_(this) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePrivetV3));
}

net::URLRequestContext* PrivetV3ContextGetter::GetURLRequestContext() {
  InitOnNetThread();
  return context_.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
PrivetV3ContextGetter::GetNetworkTaskRunner() const {
  return net_task_runner_;
}

void PrivetV3ContextGetter::InitOnNetThread() {
  DCHECK(net_task_runner_->BelongsToCurrentThread());
  if (!context_) {
    net::URLRequestContextBuilder builder;
    builder.set_proxy_service(net::ProxyService::CreateDirect());
    builder.SetSpdyAndQuicEnabled(false, false);
    builder.DisableHttpCache();
    cert_verifier_ = new CertVerifier();
    builder.SetCertVerifier(make_scoped_ptr(cert_verifier_));
    builder.set_user_agent(::GetUserAgent());
    context_ = builder.Build();
  }
}

void PrivetV3ContextGetter::AddPairedHost(
    const std::string& host,
    const net::SHA256HashValue& certificate_fingerprint,
    const base::Closure& callback) {
  net_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&PrivetV3ContextGetter::AddPairedHostOnNetThread,
                 weak_ptr_factory_.GetWeakPtr(), host, certificate_fingerprint),
      callback);
}

void PrivetV3ContextGetter::AddPairedHostOnNetThread(
    const std::string& host,
    const net::SHA256HashValue& certificate_fingerprint) {
  InitOnNetThread();
  cert_verifier_->AddPairedHost(host, certificate_fingerprint);
}

PrivetV3ContextGetter::~PrivetV3ContextGetter() {
  DCHECK(net_task_runner_->BelongsToCurrentThread());
}

}  // namespace extensions
