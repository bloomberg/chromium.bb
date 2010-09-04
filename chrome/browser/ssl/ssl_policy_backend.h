// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_POLICY_BACKEND_H_
#define CHROME_BROWSER_SSL_SSL_POLICY_BACKEND_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "net/base/x509_certificate.h"

class NavigationController;
class SSLHostState;

class SSLPolicyBackend {
 public:
  explicit SSLPolicyBackend(NavigationController* controller);

  // Records that a host has run insecure content.
  void HostRanInsecureContent(const std::string& host, int pid);

  // Returns whether the specified host ran insecure content.
  bool DidHostRunInsecureContent(const std::string& host, int pid) const;

  // Records that |cert| is permitted to be used for |host| in the future.
  void DenyCertForHost(net::X509Certificate* cert, const std::string& host);

  // Records that |cert| is not permitted to be used for |host| in the future.
  void AllowCertForHost(net::X509Certificate* cert, const std::string& host);

  // Queries whether |cert| is allowed or denied for |host|.
  net::CertPolicy::Judgment QueryPolicy(
      net::X509Certificate* cert, const std::string& host);

 private:
  // SSL state specific for each host.
  SSLHostState* ssl_host_state_;

  DISALLOW_COPY_AND_ASSIGN(SSLPolicyBackend);
};

#endif  // CHROME_BROWSER_SSL_SSL_POLICY_BACKEND_H_
