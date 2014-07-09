// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/client_cert_util.h"

#include <cert.h>
#include <pk11pub.h>

#include <list>
#include <string>
#include <vector>

#include "base/values.h"
#include "chromeos/network/certificate_pattern.h"
#include "chromeos/network/network_event_log.h"
#include "components/onc/onc_constants.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace client_cert {

namespace {

// Functor to filter out non-matching issuers.
class IssuerFilter {
 public:
  explicit IssuerFilter(const IssuerSubjectPattern& issuer)
    : issuer_(issuer) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    return !CertPrincipalMatches(issuer_, cert.get()->issuer());
  }
 private:
  const IssuerSubjectPattern& issuer_;
};

// Functor to filter out non-matching subjects.
class SubjectFilter {
 public:
  explicit SubjectFilter(const IssuerSubjectPattern& subject)
    : subject_(subject) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    return !CertPrincipalMatches(subject_, cert.get()->subject());
  }
 private:
  const IssuerSubjectPattern& subject_;
};

// Functor to filter out certs that don't have private keys, or are invalid.
class PrivateKeyFilter {
 public:
  explicit PrivateKeyFilter(net::CertDatabase* cert_db) : cert_db_(cert_db) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    return cert_db_->CheckUserCert(cert.get()) != net::OK;
  }
 private:
  net::CertDatabase* cert_db_;
};

// Functor to filter out certs that don't have an issuer in the associated
// IssuerCAPEMs list.
class IssuerCaFilter {
 public:
  explicit IssuerCaFilter(const std::vector<std::string>& issuer_ca_pems)
    : issuer_ca_pems_(issuer_ca_pems) {}
  bool operator()(const scoped_refptr<net::X509Certificate>& cert) const {
    // Find the certificate issuer for each certificate.
    // TODO(gspencer): this functionality should be available from
    // X509Certificate or NSSCertDatabase.
    net::ScopedCERTCertificate issuer_cert(CERT_FindCertIssuer(
        cert.get()->os_cert_handle(), PR_Now(), certUsageAnyCA));

    if (!issuer_cert)
      return true;

    std::string pem_encoded;
    if (!net::X509Certificate::GetPEMEncoded(issuer_cert.get(),
                                             &pem_encoded)) {
      LOG(ERROR) << "Couldn't PEM-encode certificate.";
      return true;
    }

    return (std::find(issuer_ca_pems_.begin(), issuer_ca_pems_.end(),
                      pem_encoded) ==
            issuer_ca_pems_.end());
  }
 private:
  const std::vector<std::string>& issuer_ca_pems_;
};

std::string GetStringFromDictionary(const base::DictionaryValue& dict,
                                    const std::string& key) {
  std::string s;
  dict.GetStringWithoutPathExpansion(key, &s);
  return s;
}

void GetClientCertTypeAndPattern(
    const base::DictionaryValue& dict_with_client_cert,
    ClientCertConfig* cert_config) {
  using namespace ::onc::client_cert;
  dict_with_client_cert.GetStringWithoutPathExpansion(
      kClientCertType, &cert_config->client_cert_type);

  if (cert_config->client_cert_type == kPattern) {
    const base::DictionaryValue* pattern = NULL;
    dict_with_client_cert.GetDictionaryWithoutPathExpansion(kClientCertPattern,
                                                            &pattern);
    if (pattern) {
      bool success = cert_config->pattern.ReadFromONCDictionary(*pattern);
      DCHECK(success);
    }
  }
}

}  // namespace

// Returns true only if any fields set in this pattern match exactly with
// similar fields in the principal.  If organization_ or organizational_unit_
// are set, then at least one of the organizations or units in the principal
// must match.
bool CertPrincipalMatches(const IssuerSubjectPattern& pattern,
                          const net::CertPrincipal& principal) {
  if (!pattern.common_name().empty() &&
      pattern.common_name() != principal.common_name) {
    return false;
  }

  if (!pattern.locality().empty() &&
      pattern.locality() != principal.locality_name) {
    return false;
  }

  if (!pattern.organization().empty()) {
    if (std::find(principal.organization_names.begin(),
                  principal.organization_names.end(),
                  pattern.organization()) ==
        principal.organization_names.end()) {
      return false;
    }
  }

  if (!pattern.organizational_unit().empty()) {
    if (std::find(principal.organization_unit_names.begin(),
                  principal.organization_unit_names.end(),
                  pattern.organizational_unit()) ==
        principal.organization_unit_names.end()) {
      return false;
    }
  }

  return true;
}

scoped_refptr<net::X509Certificate> GetCertificateMatch(
    const CertificatePattern& pattern,
    const net::CertificateList& all_certs) {
  typedef std::list<scoped_refptr<net::X509Certificate> > CertificateStlList;

  // Start with all the certs, and narrow it down from there.
  CertificateStlList matching_certs;

  if (all_certs.empty())
    return NULL;

  for (net::CertificateList::const_iterator iter = all_certs.begin();
       iter != all_certs.end(); ++iter) {
    matching_certs.push_back(*iter);
  }

  // Strip off any certs that don't have the right issuer and/or subject.
  if (!pattern.issuer().Empty()) {
    matching_certs.remove_if(IssuerFilter(pattern.issuer()));
    if (matching_certs.empty())
      return NULL;
  }

  if (!pattern.subject().Empty()) {
    matching_certs.remove_if(SubjectFilter(pattern.subject()));
    if (matching_certs.empty())
      return NULL;
  }

  if (!pattern.issuer_ca_pems().empty()) {
    matching_certs.remove_if(IssuerCaFilter(pattern.issuer_ca_pems()));
    if (matching_certs.empty())
      return NULL;
  }

  // Eliminate any certs that don't have private keys associated with
  // them.  The CheckUserCert call in the filter is a little slow (because of
  // underlying PKCS11 calls), so we do this last to reduce the number of times
  // we have to call it.
  PrivateKeyFilter private_filter(net::CertDatabase::GetInstance());
  matching_certs.remove_if(private_filter);

  if (matching_certs.empty())
    return NULL;

  // We now have a list of certificates that match the pattern we're
  // looking for.  Now we find the one with the latest start date.
  scoped_refptr<net::X509Certificate> latest(NULL);

  // Iterate over the rest looking for the one that was issued latest.
  for (CertificateStlList::iterator iter = matching_certs.begin();
       iter != matching_certs.end(); ++iter) {
    if (!latest.get() || (*iter)->valid_start() > latest->valid_start())
      latest = *iter;
  }

  return latest;
}

void SetShillProperties(const ConfigType cert_config_type,
                        const std::string& tpm_slot,
                        const std::string& tpm_pin,
                        const std::string* pkcs11_id,
                        base::DictionaryValue* properties) {
  const char* tpm_pin_property = NULL;
  switch (cert_config_type) {
    case CONFIG_TYPE_NONE: {
      return;
    }
    case CONFIG_TYPE_OPENVPN: {
      tpm_pin_property = shill::kOpenVPNPinProperty;
      if (pkcs11_id) {
        properties->SetStringWithoutPathExpansion(
            shill::kOpenVPNClientCertIdProperty, *pkcs11_id);
      }
      break;
    }
    case CONFIG_TYPE_IPSEC: {
      tpm_pin_property = shill::kL2tpIpsecPinProperty;
      if (!tpm_slot.empty()) {
        properties->SetStringWithoutPathExpansion(
            shill::kL2tpIpsecClientCertSlotProperty, tpm_slot);
      }
      if (pkcs11_id) {
        properties->SetStringWithoutPathExpansion(
            shill::kL2tpIpsecClientCertIdProperty, *pkcs11_id);
      }
      break;
    }
    case CONFIG_TYPE_EAP: {
      tpm_pin_property = shill::kEapPinProperty;
      if (pkcs11_id) {
        std::string key_id;
        if (pkcs11_id->empty()) {
          // An empty pkcs11_id means that we should clear the properties.
        } else {
          if (tpm_slot.empty())
            NET_LOG_ERROR("Missing TPM slot id", "");
          else
            key_id = tpm_slot + ":";
          key_id.append(*pkcs11_id);
        }
        // Shill requires both CertID and KeyID for TLS connections, despite the
        // fact that by convention they are the same ID, because one identifies
        // the certificate and the other the private key.
        properties->SetStringWithoutPathExpansion(shill::kEapCertIdProperty,
                                                  key_id);
        properties->SetStringWithoutPathExpansion(shill::kEapKeyIdProperty,
                                                  key_id);
      }
      break;
    }
  }
  DCHECK(tpm_pin_property);
  if (!tpm_pin.empty())
    properties->SetStringWithoutPathExpansion(tpm_pin_property, tpm_pin);
}

ClientCertConfig::ClientCertConfig()
    : location(CONFIG_TYPE_NONE),
      client_cert_type(onc::client_cert::kClientCertTypeNone) {
}

void OncToClientCertConfig(const base::DictionaryValue& network_config,
                           ClientCertConfig* cert_config) {
  using namespace ::onc;

  *cert_config = ClientCertConfig();

  const base::DictionaryValue* dict_with_client_cert = NULL;

  const base::DictionaryValue* wifi = NULL;
  network_config.GetDictionaryWithoutPathExpansion(network_config::kWiFi,
                                                   &wifi);
  if (wifi) {
    const base::DictionaryValue* eap = NULL;
    wifi->GetDictionaryWithoutPathExpansion(wifi::kEAP, &eap);
    if (!eap)
      return;

    dict_with_client_cert = eap;
    cert_config->location = CONFIG_TYPE_EAP;
  }

  const base::DictionaryValue* vpn = NULL;
  network_config.GetDictionaryWithoutPathExpansion(network_config::kVPN, &vpn);
  if (vpn) {
    const base::DictionaryValue* openvpn = NULL;
    vpn->GetDictionaryWithoutPathExpansion(vpn::kOpenVPN, &openvpn);
    const base::DictionaryValue* ipsec = NULL;
    vpn->GetDictionaryWithoutPathExpansion(vpn::kIPsec, &ipsec);
    if (openvpn) {
      dict_with_client_cert = openvpn;
      cert_config->location = CONFIG_TYPE_OPENVPN;
    } else if (ipsec) {
      dict_with_client_cert = ipsec;
      cert_config->location = CONFIG_TYPE_IPSEC;
    } else {
      return;
    }
  }

  const base::DictionaryValue* ethernet = NULL;
  network_config.GetDictionaryWithoutPathExpansion(network_config::kEthernet,
                                                   &ethernet);
  if (ethernet) {
    const base::DictionaryValue* eap = NULL;
    ethernet->GetDictionaryWithoutPathExpansion(wifi::kEAP, &eap);
    if (!eap)
      return;
    dict_with_client_cert = eap;
    cert_config->location = CONFIG_TYPE_EAP;
  }

  if (dict_with_client_cert)
    GetClientCertTypeAndPattern(*dict_with_client_cert, cert_config);
}

bool IsCertificateConfigured(const ConfigType cert_config_type,
                             const base::DictionaryValue& service_properties) {
  // VPN certificate properties are read from the Provider dictionary.
  const base::DictionaryValue* provider_properties = NULL;
  service_properties.GetDictionaryWithoutPathExpansion(
      shill::kProviderProperty, &provider_properties);
  switch (cert_config_type) {
    case CONFIG_TYPE_NONE:
      return true;
    case CONFIG_TYPE_OPENVPN:
      // OpenVPN generally requires a passphrase and we don't know whether or
      // not one is required, so always return false here.
      return false;
    case CONFIG_TYPE_IPSEC: {
      if (!provider_properties)
        return false;

      std::string client_cert_id;
      provider_properties->GetStringWithoutPathExpansion(
          shill::kL2tpIpsecClientCertIdProperty, &client_cert_id);
      return !client_cert_id.empty();
    }
    case CONFIG_TYPE_EAP: {
      std::string cert_id = GetStringFromDictionary(
          service_properties, shill::kEapCertIdProperty);
      std::string key_id = GetStringFromDictionary(
          service_properties, shill::kEapKeyIdProperty);
      std::string identity = GetStringFromDictionary(
          service_properties, shill::kEapIdentityProperty);
      return !cert_id.empty() && !key_id.empty() && !identity.empty();
    }
  }
  NOTREACHED();
  return false;
}

}  // namespace client_cert

}  // namespace chromeos
