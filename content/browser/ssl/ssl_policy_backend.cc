// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_policy_backend.h"

#include "content/browser/ssl/ssl_host_state.h"
#include "content/browser/web_contents/navigation_controller_impl.h"
#include "content/public/browser/browser_context.h"

namespace content {

SSLPolicyBackend::SSLPolicyBackend(NavigationControllerImpl* controller)
    : ssl_host_state_(SSLHostState::GetFor(controller->GetBrowserContext())),
      controller_(controller) {
  DCHECK(controller_);
}

void SSLPolicyBackend::HostRanInsecureContent(const std::string& host, int id) {
  ssl_host_state_->HostRanInsecureContent(host, id);
  SSLManager::NotifySSLInternalStateChanged(controller_->GetBrowserContext());
}

bool SSLPolicyBackend::DidHostRunInsecureContent(const std::string& host,
                                                 int pid) const {
  return ssl_host_state_->DidHostRunInsecureContent(host, pid);
}

void SSLPolicyBackend::DenyCertForHost(net::X509Certificate* cert,
                                       const std::string& host) {
  ssl_host_state_->DenyCertForHost(cert, host);
}

void SSLPolicyBackend::AllowCertForHost(net::X509Certificate* cert,
                                        const std::string& host) {
  ssl_host_state_->AllowCertForHost(cert, host);
}

net::CertPolicy::Judgment SSLPolicyBackend::QueryPolicy(
    net::X509Certificate* cert, const std::string& host) {
  return ssl_host_state_->QueryPolicy(cert, host);
}

}  // namespace content
