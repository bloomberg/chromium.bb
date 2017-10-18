// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_ERROR_ASSISTANT_H_
#define CHROME_BROWSER_SSL_SSL_ERROR_ASSISTANT_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "net/ssl/ssl_info.h"

namespace chrome_browser_ssl {
class SSLErrorAssistantConfig;
}  // namespace chrome_browser_ssl

namespace net {
class SSLInfo;
}

// Struct which stores data about a known MITM software pulled from the
// SSLErrorAssistant proto.
struct MITMSoftwareType {
  MITMSoftwareType(const std::string& name,
                   const std::string& issuer_common_name_regex,
                   const std::string& issuer_organization_regex);

  const std::string name;
  const std::string issuer_common_name_regex;
  const std::string issuer_organization_regex;
};

// Helper class for SSLErrorHandler. This class is responsible for reading in
// the ssl_error_assistant protobuf and parsing through the data.
class SSLErrorAssistant {
 public:
  SSLErrorAssistant();

  ~SSLErrorAssistant();

  // Returns true if any of the SHA256 hashes in |ssl_info| is of a captive
  // portal certificate. The set of captive portal hashes is loaded on first
  // use.
  bool IsKnownCaptivePortalCertificate(const net::SSLInfo& ssl_info);

  // Returns the name of a known MITM software provider that matches the
  // certificate passed in as the |cert| parameter. Returns empty string if
  // there is no match.
  const std::string MatchKnownMITMSoftware(
      const scoped_refptr<net::X509Certificate>& cert);

  void SetErrorAssistantProto(
      std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig> proto);

  // Testing methods:
  void ResetForTesting();
  int GetErrorAssistantProtoVersionIdForTesting() const;

 private:
  // SPKI hashes belonging to certs treated as captive portals. Null until the
  // first time ShouldDisplayCaptiveProtalInterstitial() or
  // SetErrorAssistantProto() is called.
  std::unique_ptr<std::unordered_set<std::string>> captive_portal_spki_hashes_;

  // Data about a known MITM software pulled from the SSLErrorAssistant proto.
  // Null until MatchKnownMITMSoftware() is called.
  std::unique_ptr<std::vector<MITMSoftwareType>> mitm_software_list_;

  // Error assistant configuration.
  std::unique_ptr<chrome_browser_ssl::SSLErrorAssistantConfig>
      error_assistant_proto_;

  DISALLOW_COPY_AND_ASSIGN(SSLErrorAssistant);
};

#endif  // CHROME_BROWSER_SSL_SSL_ERROR_ASSISTANT_H_
