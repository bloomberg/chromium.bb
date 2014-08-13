// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_policy_backend.h"

#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/ssl_host_state_delegate.h"

namespace content {

SSLPolicyBackend::SSLPolicyBackend(NavigationControllerImpl* controller)
    : ssl_host_state_delegate_(
          controller->GetBrowserContext()->GetSSLHostStateDelegate()),
      controller_(controller) {
  DCHECK(controller_);
}

void SSLPolicyBackend::HostRanInsecureContent(const std::string& host, int id) {
  if (ssl_host_state_delegate_)
    ssl_host_state_delegate_->HostRanInsecureContent(host, id);
  SSLManager::NotifySSLInternalStateChanged(controller_->GetBrowserContext());
}

bool SSLPolicyBackend::DidHostRunInsecureContent(const std::string& host,
                                                 int pid) const {
  if (!ssl_host_state_delegate_)
    return false;

  return ssl_host_state_delegate_->DidHostRunInsecureContent(host, pid);
}

void SSLPolicyBackend::DenyCertForHost(net::X509Certificate* cert,
                                       const std::string& host,
                                       net::CertStatus error) {
  if (ssl_host_state_delegate_)
    ssl_host_state_delegate_->DenyCert(host, cert, error);
}

void SSLPolicyBackend::AllowCertForHost(net::X509Certificate* cert,
                                        const std::string& host,
                                        net::CertStatus error) {
  if (ssl_host_state_delegate_)
    ssl_host_state_delegate_->AllowCert(host, cert, error);
}

net::CertPolicy::Judgment SSLPolicyBackend::QueryPolicy(
    net::X509Certificate* cert,
    const std::string& host,
    net::CertStatus error) {
  if (!ssl_host_state_delegate_)
    return net::CertPolicy::UNKNOWN;

  return ssl_host_state_delegate_->QueryPolicy(host, cert, error);
}

}  // namespace content
