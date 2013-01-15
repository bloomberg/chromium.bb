// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_normalizer.h"

#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"

namespace chromeos {
namespace onc {

Normalizer::Normalizer(bool remove_recommended_fields)
    : remove_recommended_fields_(remove_recommended_fields) {
}

Normalizer::~Normalizer() {
}

scoped_ptr<base::DictionaryValue> Normalizer::NormalizeObject(
    const OncValueSignature* object_signature,
    const base::DictionaryValue& onc_object) {
  CHECK(object_signature != NULL);
  bool error = false;
  scoped_ptr<base::DictionaryValue> result =
      MapObject(*object_signature, onc_object, &error);
  DCHECK(!error);
  return result.Pass();
}

scoped_ptr<base::DictionaryValue> Normalizer::MapObject(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object,
    bool* error) {
  scoped_ptr<base::DictionaryValue> normalized =
      Mapper::MapObject(signature, onc_object, error);

  if (normalized.get() == NULL)
    return scoped_ptr<base::DictionaryValue>();

  if (remove_recommended_fields_)
    normalized->RemoveWithoutPathExpansion(kRecommended, NULL);

  if (&signature == &kCertificateSignature)
    NormalizeCertificate(normalized.get());
  else if (&signature == &kEAPSignature)
    NormalizeEAP(normalized.get());
  else if (&signature == &kEthernetSignature)
    NormalizeEthernet(normalized.get());
  else if (&signature == &kIPsecSignature)
    NormalizeIPsec(normalized.get());
  else if (&signature == &kNetworkConfigurationSignature)
    NormalizeNetworkConfiguration(normalized.get());
  else if (&signature == &kOpenVPNSignature)
    NormalizeOpenVPN(normalized.get());
  else if (&signature == &kProxySettingsSignature)
    NormalizeProxySettings(normalized.get());
  else if (&signature == &kVPNSignature)
    NormalizeVPN(normalized.get());
  else if (&signature == &kWiFiSignature)
    NormalizeWiFi(normalized.get());

  return normalized.Pass();
}

namespace {

void RemoveEntryUnless(base::DictionaryValue* dict,
                       const std::string path,
                       bool condition) {
  if (!condition)
    dict->RemoveWithoutPathExpansion(path, NULL);
}

}  // namespace

void Normalizer::NormalizeCertificate(base::DictionaryValue* cert) {
  using namespace certificate;

  bool remove = false;
  cert->GetBooleanWithoutPathExpansion(kRemove, &remove);
  RemoveEntryUnless(cert, certificate::kType, !remove);

  std::string type;
  cert->GetStringWithoutPathExpansion(certificate::kType, &type);
  RemoveEntryUnless(cert, kPKCS12, type == kClient);
  RemoveEntryUnless(cert, kTrust, type == kServer || type == kAuthority);
  RemoveEntryUnless(cert, kX509, type == kServer || type == kAuthority);
}

void Normalizer::NormalizeEthernet(base::DictionaryValue* ethernet) {
  using namespace ethernet;

  std::string auth;
  ethernet->GetStringWithoutPathExpansion(kAuthentication, &auth);
  RemoveEntryUnless(ethernet, kEAP, auth == k8021X);
}

void Normalizer::NormalizeEAP(base::DictionaryValue* eap) {
  using namespace eap;

  std::string clientcert_type;
  eap->GetStringWithoutPathExpansion(kClientCertType, &clientcert_type);
  RemoveEntryUnless(eap, kClientCertPattern,
                    clientcert_type == certificate::kPattern);
  RemoveEntryUnless(eap, kClientCertRef, clientcert_type == certificate::kRef);

  std::string outer;
  eap->GetStringWithoutPathExpansion(kOuter, &outer);
  RemoveEntryUnless(eap, kAnonymousIdentity,
                    outer == kPEAP || outer == kEAP_TTLS);
  RemoveEntryUnless(eap, kInner,
                    outer == kPEAP || outer == kEAP_TTLS || outer == kEAP_FAST);
}

void Normalizer::NormalizeIPsec(base::DictionaryValue* ipsec) {
  using namespace vpn;

  std::string auth_type;
  ipsec->GetStringWithoutPathExpansion(kAuthenticationType, &auth_type);
  RemoveEntryUnless(ipsec, kClientCertType, auth_type == kCert);
  RemoveEntryUnless(ipsec, kServerCARef, auth_type == kCert);
  RemoveEntryUnless(ipsec, kPSK, auth_type == kPSK);
  RemoveEntryUnless(ipsec, kSaveCredentials, auth_type == kPSK);

  std::string clientcert_type;
  ipsec->GetStringWithoutPathExpansion(kClientCertType, &clientcert_type);
  RemoveEntryUnless(ipsec, kClientCertPattern,
                    clientcert_type == certificate::kPattern);
  RemoveEntryUnless(ipsec, kClientCertRef,
                    clientcert_type == certificate::kRef);

  int ike_version = -1;
  ipsec->GetIntegerWithoutPathExpansion(kIKEVersion, &ike_version);
  RemoveEntryUnless(ipsec, kEAP, ike_version == 2);
  RemoveEntryUnless(ipsec, kGroup, ike_version == 1);
  RemoveEntryUnless(ipsec, kXAUTH, ike_version == 1);
}

void Normalizer::NormalizeNetworkConfiguration(base::DictionaryValue* network) {
  bool remove = false;
  network->GetBooleanWithoutPathExpansion(kRemove, &remove);
  if (remove) {
    network->RemoveWithoutPathExpansion(kIPConfigs, NULL);
    network->RemoveWithoutPathExpansion(kName, NULL);
    network->RemoveWithoutPathExpansion(kNameServers, NULL);
    network->RemoveWithoutPathExpansion(kProxySettings, NULL);
    network->RemoveWithoutPathExpansion(kSearchDomains, NULL);
    network->RemoveWithoutPathExpansion(kType, NULL);
    // Fields dependent on kType are removed afterwards, too.
  }

  std::string type;
  network->GetStringWithoutPathExpansion(kType, &type);
  RemoveEntryUnless(network, kEthernet, type == kEthernet);
  RemoveEntryUnless(network, kVPN, type == kVPN);
  RemoveEntryUnless(network, kWiFi, type == kWiFi);
}

void Normalizer::NormalizeOpenVPN(base::DictionaryValue* openvpn) {
  using namespace vpn;

  std::string clientcert_type;
  openvpn->GetStringWithoutPathExpansion(kClientCertType, &clientcert_type);
  RemoveEntryUnless(openvpn, kClientCertPattern,
                    clientcert_type == certificate::kPattern);
  RemoveEntryUnless(openvpn, kClientCertRef,
                    clientcert_type == certificate::kRef);
}

void Normalizer::NormalizeProxySettings(base::DictionaryValue* proxy) {
  using namespace proxy;

  std::string type;
  proxy->GetStringWithoutPathExpansion(proxy::kType, &type);
  RemoveEntryUnless(proxy, kManual, type == kManual);
  RemoveEntryUnless(proxy, kExcludeDomains, type == kManual);
  RemoveEntryUnless(proxy, kPAC, type == kPAC);
}

void Normalizer::NormalizeVPN(base::DictionaryValue* vpn) {
  using namespace vpn;

  std::string type;
  vpn->GetStringWithoutPathExpansion(vpn::kType, &type);
  RemoveEntryUnless(vpn, kOpenVPN, type == kOpenVPN);
  RemoveEntryUnless(vpn, kIPsec, type == kIPsec || type == kTypeL2TP_IPsec);
  RemoveEntryUnless(vpn, kL2TP, type == kTypeL2TP_IPsec);
}

void Normalizer::NormalizeWiFi(base::DictionaryValue* wifi) {
  using namespace wifi;

  std::string security;
  wifi->GetStringWithoutPathExpansion(wifi::kSecurity, &security);
  RemoveEntryUnless(wifi, kEAP, security == kWEP_8021X || security == kWPA_EAP);
  RemoveEntryUnless(wifi, kPassphrase,
                    security == kWEP_PSK || security == kWPA_PSK);
}

}  // namespace onc
}  // namespace chromeos
