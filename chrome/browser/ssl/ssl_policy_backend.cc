// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_policy_backend.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_host_state.h"
#include "content/browser/tab_contents/navigation_controller.h"

SSLPolicyBackend::SSLPolicyBackend(NavigationController* controller)
    : ssl_host_state_(controller->profile()->GetSSLHostState()) {
  DCHECK(controller);
}

void SSLPolicyBackend::HostRanInsecureContent(const std::string& host, int id) {
  ssl_host_state_->HostRanInsecureContent(host, id);
  SSLManager::NotifySSLInternalStateChanged();
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
