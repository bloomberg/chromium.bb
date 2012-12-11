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
  return MapObject(*object_signature, onc_object);
}

scoped_ptr<base::DictionaryValue> Normalizer::MapObject(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object) {
  scoped_ptr<base::DictionaryValue> normalized =
      Mapper::MapObject(signature, onc_object);

  if (normalized.get() == NULL)
    return scoped_ptr<base::DictionaryValue>();

  if (remove_recommended_fields_)
    normalized->RemoveWithoutPathExpansion(kRecommended, NULL);

  if (&signature == &kNetworkConfigurationSignature)
    NormalizeNetworkConfiguration(normalized.get());
  else if (&signature == &kVPNSignature)
    NormalizeVPN(normalized.get());
  else if (&signature == &kIPsecSignature)
    NormalizeIPsec(normalized.get());

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

void Normalizer::NormalizeVPN(base::DictionaryValue* vpn) {
  using namespace vpn;
  std::string type;
  vpn->GetStringWithoutPathExpansion(vpn::kType, &type);
  RemoveEntryUnless(vpn, kOpenVPN, type == kOpenVPN);
  RemoveEntryUnless(vpn, kIPsec, type == kIPsec || type == kTypeL2TP_IPsec);
  RemoveEntryUnless(vpn, kL2TP, type == kTypeL2TP_IPsec);
}

void Normalizer::NormalizeNetworkConfiguration(base::DictionaryValue* network) {
  std::string type;
  network->GetStringWithoutPathExpansion(kType, &type);
  RemoveEntryUnless(network, kEthernet, type == kEthernet);
  RemoveEntryUnless(network, kVPN, type == kVPN);
  RemoveEntryUnless(network, kWiFi, type == kWiFi);
}

}  // namespace onc
}  // namespace chromeos
