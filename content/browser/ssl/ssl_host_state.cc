// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_host_state.h"

#include "base/logging.h"
#include "base/lazy_instance.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

const char kKeyName[] = "content_ssl_host_state";

namespace content {

SSLHostState* SSLHostState::GetFor(BrowserContext* context) {
  SSLHostState* rv = static_cast<SSLHostState*>(context->GetUserData(kKeyName));
  if (!rv) {
    rv = new SSLHostState();
    rv->delegate_ = context->GetSSLHostStateDelegate();
    // |context| may be NULL, implementing the default storage strategy.
    if (context)
      context->SetUserData(kKeyName, rv);
  }
  return rv;
}

SSLHostState::SSLHostState() {
}

SSLHostState::~SSLHostState() {
}

void SSLHostState::HostRanInsecureContent(const std::string& host, int pid) {
  DCHECK(CalledOnValidThread());
  ran_insecure_content_hosts_.insert(BrokenHostEntry(host, pid));
}

bool SSLHostState::DidHostRunInsecureContent(const std::string& host,
                                             int pid) const {
  DCHECK(CalledOnValidThread());
  return !!ran_insecure_content_hosts_.count(BrokenHostEntry(host, pid));
}

void SSLHostState::DenyCertForHost(net::X509Certificate* cert,
                                   const std::string& host,
                                   net::CertStatus error) {
  DCHECK(CalledOnValidThread());

  if (!delegate_)
    return;

  delegate_->DenyCert(host, cert, error);
}

void SSLHostState::AllowCertForHost(net::X509Certificate* cert,
                                    const std::string& host,
                                    net::CertStatus error) {
  DCHECK(CalledOnValidThread());

  if (!delegate_)
    return;

  delegate_->AllowCert(host, cert, error);
}

void SSLHostState::RevokeAllowAndDenyPreferences(const std::string& host) {
  DCHECK(CalledOnValidThread());

  if (!delegate_)
    return;

  // TODO(jww): This will revoke all of the decisions in the browser context.
  // However, the networking stack actually keeps track of its own list of
  // exceptions per-HttpNetworkTransaction in the SSLConfig structure (see the
  // allowed_bad_certs Vector in net/ssl/ssl_config.h). This dual-tracking of
  // exceptions introduces a problem where the browser context can revoke a
  // certificate, but if a transaction reuses a cached version of the SSLConfig
  // (probably from a pooled socket), it may bypass the intestitial layer.
  //
  // Over time, the cached versions should expire and it should converge on
  // showing the interstitial. We probably need to
  // introduce into the networking stack a way revoke SSLConfig's
  // allowed_bad_certs lists per socket.
  delegate_->RevokeAllowAndDenyPreferences(host);
}

bool SSLHostState::HasAllowedOrDeniedCert(const std::string& host) {
  DCHECK(CalledOnValidThread());

  if (!delegate_)
    return false;

  return delegate_->HasAllowedOrDeniedCert(host);
}

void SSLHostState::Clear() {
  if (!delegate_)
    return;

  delegate_->Clear();
}

net::CertPolicy::Judgment SSLHostState::QueryPolicy(net::X509Certificate* cert,
                                                    const std::string& host,
                                                    net::CertStatus error) {
  DCHECK(CalledOnValidThread());

  if (!delegate_)
    return net::CertPolicy::Judgment::UNKNOWN;

  return delegate_->QueryPolicy(host, cert, error);
}

}  // namespace content
