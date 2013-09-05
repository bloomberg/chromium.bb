// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_
#define CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chromeos/chromeos_export.h"

namespace base {
class DictionaryValue;
}

namespace net {
struct CertPrincipal;
class X509Certificate;
}

namespace chromeos {

class CertificatePattern;
class IssuerSubjectPattern;

namespace client_cert {

enum ConfigType {
  CONFIG_TYPE_NONE,
  CONFIG_TYPE_OPENVPN,
  CONFIG_TYPE_IPSEC,
  CONFIG_TYPE_EAP
};

// Returns true only if any fields set in this pattern match exactly with
// similar fields in the principal.  If organization_ or organizational_unit_
// are set, then at least one of the organizations or units in the principal
// must match.
bool CertPrincipalMatches(const IssuerSubjectPattern& pattern,
                          const net::CertPrincipal& principal);

// Fetches the matching certificate that has the latest valid start date.
// Returns a NULL refptr if there is no such match.
CHROMEOS_EXPORT scoped_refptr<net::X509Certificate> GetCertificateMatch(
    const CertificatePattern& pattern);

// If not empty, sets the TPM properties in |properties|. If |pkcs11_id| is not
// NULL, also sets the ClientCertID. |cert_config_type| determines which
// dictionary entries to set.
void SetShillProperties(const ConfigType cert_config_type,
                        const std::string& tpm_slot,
                        const std::string& tpm_pin,
                        const std::string* pkcs11_id,
                        base::DictionaryValue* properties);

// Returns true if all required configuration properties are set and not empty.
bool IsCertificateConfigured(const client_cert::ConfigType cert_config_type,
                             const base::DictionaryValue& service_properties);

}  // namespace client_cert

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_
