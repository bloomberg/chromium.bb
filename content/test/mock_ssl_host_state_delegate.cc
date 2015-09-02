// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_ssl_host_state_delegate.h"

namespace content {

MockSSLHostStateDelegate::MockSSLHostStateDelegate() {}

MockSSLHostStateDelegate::~MockSSLHostStateDelegate() {}

void MockSSLHostStateDelegate::AllowCert(const std::string& host,
                                         const net::X509Certificate& cert,
                                         net::CertStatus error) {
  exceptions_.insert(host);
}

void MockSSLHostStateDelegate::Clear() {
  exceptions_.clear();
}

SSLHostStateDelegate::CertJudgment MockSSLHostStateDelegate::QueryPolicy(
    const std::string& host,
    const net::X509Certificate& cert,
    net::CertStatus error,
    bool* expired_previous_decision) {
  if (exceptions_.find(host) == exceptions_.end())
    return SSLHostStateDelegate::DENIED;

  return SSLHostStateDelegate::ALLOWED;
}

void MockSSLHostStateDelegate::HostRanInsecureContent(const std::string& host,
                                                      int pid) {}

bool MockSSLHostStateDelegate::DidHostRunInsecureContent(
    const std::string& host,
    int pid) const {
  return false;
}

void MockSSLHostStateDelegate::RevokeUserAllowExceptions(
    const std::string& host) {
  exceptions_.erase(exceptions_.find(host));
}

bool MockSSLHostStateDelegate::HasAllowException(
    const std::string& host) const {
  return exceptions_.find(host) != exceptions_.end();
}

}  // namespace content
