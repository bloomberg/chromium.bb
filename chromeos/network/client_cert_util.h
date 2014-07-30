// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_
#define CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/certificate_pattern.h"

namespace base {
class DictionaryValue;
}

namespace net {
struct CertPrincipal;
class X509Certificate;
typedef std::vector<scoped_refptr<X509Certificate> > CertificateList;
}

namespace chromeos {

namespace client_cert {

enum ConfigType {
  CONFIG_TYPE_NONE,
  CONFIG_TYPE_OPENVPN,
  CONFIG_TYPE_IPSEC,
  CONFIG_TYPE_EAP
};

struct CHROMEOS_EXPORT ClientCertConfig {
  ClientCertConfig();

  // Independent of whether the client cert (pattern or reference) is
  // configured, the location determines whether this network configuration
  // supports client certs and what kind of configuration it requires.
  ConfigType location;

  // One of the ClientCertTypes defined in ONC: kNone, kRef, or kPattern.
  std::string client_cert_type;

  // If |client_cert_type| equals kPattern, this contains the pattern.
  CertificatePattern pattern;
};

// Returns true only if any fields set in this pattern match exactly with
// similar fields in the principal.  If organization_ or organizational_unit_
// are set, then at least one of the organizations or units in the principal
// must match.
bool CertPrincipalMatches(const IssuerSubjectPattern& pattern,
                          const net::CertPrincipal& principal);

// Returns the PKCS11 id part of |cert_id|, which is expected to be the value of
// the Shill property kEapCertIdProperty or kEapKeyIdProperty.
CHROMEOS_EXPORT std::string GetPkcs11IdFromEapCertId(
    const std::string& cert_id);

// Sets the properties of a client cert and the TPM slot that it's contained in.
// |cert_config_type| determines which dictionary entries to set.
CHROMEOS_EXPORT void SetShillProperties(const ConfigType cert_config_type,
                                        const int tpm_slot,
                                        const std::string& pkcs11_id,
                                        base::DictionaryValue* properties);

// Like SetShillProperties but instead sets the properties to empty strings.
// This should be used to clear previously set client certificate properties.
CHROMEOS_EXPORT void SetEmptyShillProperties(const ConfigType cert_config_type,
                                             base::DictionaryValue* properties);

// Returns true if all required configuration properties are set and not empty.
bool IsCertificateConfigured(const client_cert::ConfigType cert_config_type,
                             const base::DictionaryValue& service_properties);

// Determines the type of the CertificatePattern configuration, i.e. is it a
// pattern within an EAP, IPsec or OpenVPN configuration.
CHROMEOS_EXPORT void OncToClientCertConfig(
    const base::DictionaryValue& network_config,
    ClientCertConfig* cert_config);

}  // namespace client_cert

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CLIENT_CERT_UTIL_H_
