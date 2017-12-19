// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_UNTRUSTED_AUTHORITY_CERTS_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_UNTRUSTED_AUTHORITY_CERTS_CACHE_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "net/cert/scoped_nss_types.h"

namespace policy {

// Holds untrusted intermediate authority certificates in memory as
// ScopedCERTCertificates, making them available for client certificate
// discovery.
class UntrustedAuthorityCertsCache {
 public:
  explicit UntrustedAuthorityCertsCache(
      const std::vector<std::string>& onc_x509_authority_certs);
  ~UntrustedAuthorityCertsCache();

  static std::vector<std::string> GetUntrustedAuthoritiesFromDeviceOncPolicy();

 private:
  // The actual cache of untrusted authorities.
  // Don't delete this field, even if it looks unused!
  // This is a list which owns ScopedCERTCertificate objects. This is sufficient
  // for NSS to be able to find them using CERT_FindCertByName, which is enough
  // for them to be used as intermediate certificates during client certificate
  // matching. Note that when the ScopedCERTCertificate objects go out of scope,
  // they don't necessarily become unavailable in NSS due to caching behavior.
  // However, this is not an issue, as these certificates are not imported into
  // permanent databases, nor are the trust settings mutated to trust them.
  net::ScopedCERTCertificateList untrusted_authority_certs_;

  DISALLOW_COPY_AND_ASSIGN(UntrustedAuthorityCertsCache);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_UNTRUSTED_AUTHORITY_CERTS_CACHE_H_
