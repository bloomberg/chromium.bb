// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_validator.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"

namespace chromeos {
namespace onc {

Validator::Validator(
    bool error_on_unknown_field,
    bool error_on_wrong_recommended,
    bool error_on_missing_field,
    bool managed_onc)
    : error_on_unknown_field_(error_on_unknown_field),
      error_on_wrong_recommended_(error_on_wrong_recommended),
      error_on_missing_field_(error_on_missing_field),
      managed_onc_(managed_onc) {
}

Validator::~Validator() {
}

scoped_ptr<base::DictionaryValue> Validator::ValidateAndRepairObject(
    const OncValueSignature* object_signature,
    const base::DictionaryValue& onc_object) {
  CHECK(object_signature != NULL);
  scoped_ptr<base::Value> result_value =
      MapValue(*object_signature, onc_object);
  base::DictionaryValue* result_dict = NULL;
  if (result_value.get() != NULL) {
    result_value.release()->GetAsDictionary(&result_dict);
    CHECK(result_dict != NULL);
  }

  return make_scoped_ptr(result_dict);
}

scoped_ptr<base::Value> Validator::MapValue(
    const OncValueSignature& signature,
    const base::Value& onc_value) {
  if (onc_value.GetType() != signature.onc_type) {
    DVLOG(1) << "Wrong type. Expected " << signature.onc_type
             << ", but found " << onc_value.GetType();
    return scoped_ptr<base::Value>();
  }

  scoped_ptr<base::Value> repaired = Mapper::MapValue(signature, onc_value);
  if (repaired.get() != NULL)
    CHECK_EQ(repaired->GetType(), signature.onc_type);
  return repaired.Pass();
}

scoped_ptr<base::DictionaryValue> Validator::MapObject(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object) {
  scoped_ptr<base::DictionaryValue> repaired(new base::DictionaryValue);

  bool valid;
  if (&signature == &kNetworkConfigurationSignature)
    valid = ValidateNetworkConfiguration(onc_object, repaired.get());
  else if (&signature == &kEthernetSignature)
    valid = ValidateEthernet(onc_object, repaired.get());
  else if (&signature == &kIPConfigSignature)
    valid = ValidateIPConfig(onc_object, repaired.get());
  else if (&signature == &kWiFiSignature)
    valid = ValidateWiFi(onc_object, repaired.get());
  else if (&signature == &kVPNSignature)
    valid = ValidateVPN(onc_object, repaired.get());
  else if (&signature == &kIPsecSignature)
    valid = ValidateIPsec(onc_object, repaired.get());
  else if (&signature == &kOpenVPNSignature)
    valid = ValidateOpenVPN(onc_object, repaired.get());
  else if (&signature == &kCertificatePatternSignature)
    valid = ValidateCertificatePattern(onc_object, repaired.get());
  else if (&signature == &kProxySettingsSignature)
    valid = ValidateProxySettings(onc_object, repaired.get());
  else if (&signature == &kProxyLocationSignature)
    valid = ValidateProxyLocation(onc_object, repaired.get());
  else if (&signature == &kEAPSignature)
    valid = ValidateEAP(onc_object, repaired.get());
  else if (&signature == &kCertificateSignature)
    valid = ValidateCertificate(onc_object, repaired.get());
  else
    valid = ValidateObjectDefault(signature, onc_object, repaired.get());

  if (valid)
    return repaired.Pass();
  else
    return scoped_ptr<base::DictionaryValue>();
}

bool Validator::ValidateObjectDefault(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  bool found_unknown_field = false;
  bool nested_error_occured = false;
  MapFields(signature, onc_object, &found_unknown_field, &nested_error_occured,
            result);
  if (nested_error_occured)
    return false;

  if (found_unknown_field) {
    if (error_on_unknown_field_) {
      DVLOG(1) << "Unknown field name. Aborting.";
      return false;
    }
    DVLOG(1) << "Unknown field name. Ignoring.";
  }

  return ValidateRecommendedField(signature, result);
}

bool Validator::ValidateRecommendedField(
    const OncValueSignature& object_signature,
    base::DictionaryValue* result) {
  CHECK(result != NULL);

  scoped_ptr<base::ListValue> recommended;
  base::Value* recommended_value;
  // This remove passes ownership to |recommended_value|.
  if (!result->RemoveWithoutPathExpansion(onc::kRecommended,
                                          &recommended_value)) {
    return true;
  }
  base::ListValue* recommended_list;
  recommended_value->GetAsList(&recommended_list);
  CHECK(recommended_list != NULL);

  recommended.reset(recommended_list);

  if (!managed_onc_) {
    DVLOG(1) << "Found a " << onc::kRecommended
             << " field in unmanaged ONC. Removing it.";
    return true;
  }

  scoped_ptr<base::ListValue> repaired_recommended(new base::ListValue);
  for (base::ListValue::iterator it = recommended->begin();
       it != recommended->end(); ++it) {
    std::string field_name;
    if (!(*it)->GetAsString(&field_name)) {
      NOTREACHED();
      continue;
    }

    const OncFieldSignature* field_signature =
        GetFieldSignature(object_signature, field_name);

    bool found_error = false;
    std::string error_cause;
    if (field_signature == NULL) {
      found_error = true;
      error_cause = "unknown";
    } else if (field_signature->value_signature->onc_type ==
               base::Value::TYPE_DICTIONARY) {
      found_error = true;
      error_cause = "dictionary-typed";
    }

    if (found_error) {
      DVLOG(1) << "Found " << error_cause << " field name '" << field_name
               << "' in kRecommended array. "
               << (error_on_wrong_recommended_ ? "Aborting." : "Ignoring.");
      if (error_on_wrong_recommended_)
        return false;
      else
        continue;
    }

    repaired_recommended->Append((*it)->DeepCopy());
  }

  result->Set(onc::kRecommended, repaired_recommended.release());
  return true;
}

namespace {

std::string JoinStringRange(const char** range_begin,
                            const char** range_end,
                            const std::string& separator) {
  std::vector<std::string> string_vector;
  std::copy(range_begin, range_end, std::back_inserter(string_vector));
  return JoinString(string_vector, separator);
}

bool RequireAnyOf(const std::string &actual, const char** valid_values) {
  const char** it = valid_values;
  for (; *it != NULL; ++it) {
    if (actual == *it)
      return true;
  }
  DVLOG(1) << "Found " << actual << ", but expected one of "
           << JoinStringRange(valid_values, it, ", ");
  return false;
}

bool IsInRange(int actual, int lower_bound, int upper_bound) {
  if (lower_bound <= actual && actual <= upper_bound)
    return true;
  DVLOG(1) << "Found " << actual << ", which is out of range [" << lower_bound
           << ", " << upper_bound << "]";
  return false;
}

bool RequireField(const base::DictionaryValue& dict, std::string key) {
  if (dict.HasKey(key))
    return true;
  DVLOG(1) << "Required field " << key << " missing.";
  return false;
}

}  // namespace

bool Validator::ValidateNetworkConfiguration(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  if (!ValidateObjectDefault(kNetworkConfigurationSignature,
                             onc_object, result)) {
    return false;
  }

  std::string type;
  static const char* kValidTypes[] = { kEthernet, kVPN, kWiFi, NULL };
  if (result->GetStringWithoutPathExpansion(kType, &type) &&
      !RequireAnyOf(type, kValidTypes)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, kGUID);

  bool remove = false;
  result->GetBooleanWithoutPathExpansion(kRemove, &remove);
  if (!remove) {
    allRequiredExist &= RequireField(*result, kName);
    allRequiredExist &= RequireField(*result, kType);
    allRequiredExist &= type.empty() || RequireField(*result, type);
  }

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateEthernet(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  using namespace onc::ethernet;
  if (!ValidateObjectDefault(kEthernetSignature, onc_object, result))
    return false;

  std::string auth;
  static const char* kValidAuthentications[] = { kNone, k8021X, NULL };
  if (result->GetStringWithoutPathExpansion(kAuthentication, &auth) &&
      !RequireAnyOf(auth, kValidAuthentications)) {
    return false;
  }

  bool allRequiredExist = true;
  if (auth == k8021X)
    allRequiredExist &= RequireField(*result, kEAP);

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateIPConfig(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  using namespace onc::ipconfig;
  if (!ValidateObjectDefault(kIPConfigSignature, onc_object, result))
    return false;

  std::string type;
  static const char* kValidTypes[] = { kIPv4, kIPv6, NULL };
  if (result->GetStringWithoutPathExpansion(ipconfig::kType, &type) &&
      !RequireAnyOf(type, kValidTypes)) {
    return false;
  }

  int routing_prefix;
  int lower_bound = 1;
  // In case of missing type, choose higher upper_bound.
  int upper_bound = (type == kIPv4) ? 32 : 128;
  if (result->GetIntegerWithoutPathExpansion(kRoutingPrefix, &routing_prefix) &&
      !IsInRange(routing_prefix, lower_bound, upper_bound)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, kIPAddress) &
      RequireField(*result, kRoutingPrefix) &
      RequireField(*result, ipconfig::kType);

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateWiFi(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  using namespace onc::wifi;
  if (!ValidateObjectDefault(kWiFiSignature, onc_object, result))
    return false;

  std::string security;
  static const char* kValidSecurities[] =
      { kNone, kWEP_PSK, kWEP_8021X, kWPA_PSK, kWPA_EAP, NULL };
  if (result->GetStringWithoutPathExpansion(kSecurity, &security) &&
      !RequireAnyOf(security, kValidSecurities)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, kSecurity) &
      RequireField(*result, kSSID);
  if (security == kWEP_8021X || security == kWPA_EAP)
    allRequiredExist &= RequireField(*result, kEAP);
  else if (security == kWEP_PSK || security == kWPA_PSK)
    allRequiredExist &= RequireField(*result, kPassphrase);

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateVPN(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  using namespace vpn;
  if (!ValidateObjectDefault(kVPNSignature, onc_object, result))
    return false;

  std::string type;
  static const char* kValidTypes[] =
      { kIPsec, kTypeL2TP_IPsec, kOpenVPN, NULL };
  if (result->GetStringWithoutPathExpansion(vpn::kType, &type) &&
      !RequireAnyOf(type, kValidTypes)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, vpn::kType);

  if (type == kOpenVPN) {
    allRequiredExist &= RequireField(*result, kOpenVPN);
  } else if (type == kIPsec) {
    allRequiredExist &= RequireField(*result, kIPsec);
  } else if (type == kTypeL2TP_IPsec) {
    allRequiredExist &= RequireField(*result, kIPsec) &
        RequireField(*result, kL2TP);
  }

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateIPsec(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  using namespace onc::vpn;
  using namespace onc::certificate;
  if (!ValidateObjectDefault(kIPsecSignature, onc_object, result))
    return false;

  std::string auth;
  static const char* kValidAuthentications[] = { kPSK, kCert, NULL };
  if (result->GetStringWithoutPathExpansion(kAuthenticationType, &auth) &&
      !RequireAnyOf(auth, kValidAuthentications)) {
    return false;
  }

  std::string cert_type;
  static const char* kValidCertTypes[] = { kRef, kPattern, NULL };
  if (result->GetStringWithoutPathExpansion(kClientCertType, &cert_type) &&
      !RequireAnyOf(cert_type, kValidCertTypes)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, kAuthenticationType) &
      RequireField(*result, kIKEVersion);
  if (auth == kCert) {
    allRequiredExist &= RequireField(*result, kClientCertType) &
        RequireField(*result, kServerCARef);
  }
  if (cert_type == kPattern)
    allRequiredExist &= RequireField(*result, kClientCertPattern);
  else if (cert_type == kRef)
    allRequiredExist &= RequireField(*result, kClientCertRef);

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateOpenVPN(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  using namespace onc::vpn;
  using namespace onc::openvpn;
  using namespace onc::certificate;
  if (!ValidateObjectDefault(kOpenVPNSignature, onc_object, result))
    return false;

  std::string auth_retry;
  static const char* kValidAuthRetryValues[] =
      { openvpn::kNone, kInteract, kNoInteract, NULL };
  if (result->GetStringWithoutPathExpansion(kAuthRetry, &auth_retry) &&
      !RequireAnyOf(auth_retry, kValidAuthRetryValues)) {
    return false;
  }

  std::string cert_type;
  static const char* kValidCertTypes[] =
      { certificate::kNone, kRef, kPattern, NULL };
  if (result->GetStringWithoutPathExpansion(kClientCertType, &cert_type) &&
      !RequireAnyOf(cert_type, kValidCertTypes)) {
    return false;
  }

  std::string cert_tls;
  static const char* kValidCertTlsValues[] =
      { openvpn::kNone, openvpn::kServer, NULL };
  if (result->GetStringWithoutPathExpansion(kRemoteCertTLS, &cert_tls) &&
      !RequireAnyOf(cert_tls, kValidCertTlsValues)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, kClientCertType);
  if (cert_type == kPattern)
    allRequiredExist &= RequireField(*result, kClientCertPattern);
  else if (cert_type == kRef)
    allRequiredExist &= RequireField(*result, kClientCertRef);

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateCertificatePattern(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  using namespace onc::certificate;
  if (!ValidateObjectDefault(kCertificatePatternSignature, onc_object, result))
    return false;

  bool allRequiredExist = true;
  if (!result->HasKey(kSubject) && !result->HasKey(kIssuer) &&
      !result->HasKey(kIssuerCARef)) {
    allRequiredExist = false;
    DVLOG(1) << "None of the fields " << kSubject << ", " << kIssuer << ", and "
             << kIssuerCARef << " exists, but at least one is required.";
  }

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateProxySettings(const base::DictionaryValue& onc_object,
                                      base::DictionaryValue* result) {
  using namespace onc::proxy;
  if (!ValidateObjectDefault(kProxySettingsSignature, onc_object, result))
    return false;

  std::string type;
  static const char* kValidTypes[] = { kDirect, kManual, kPAC, kWPAD, NULL };
  if (result->GetStringWithoutPathExpansion(proxy::kType, &type) &&
      !RequireAnyOf(type, kValidTypes)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, proxy::kType);

  if (type == kManual)
    allRequiredExist &= RequireField(*result, kManual);
  else if (type == kPAC)
    allRequiredExist &= RequireField(*result, kPAC);

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateProxyLocation(const base::DictionaryValue& onc_object,
                                      base::DictionaryValue* result) {
  using namespace onc::proxy;
  if (!ValidateObjectDefault(kProxyLocationSignature, onc_object, result))
    return false;

  bool allRequiredExist = RequireField(*result, kHost) &
      RequireField(*result, kPort);

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateEAP(const base::DictionaryValue& onc_object,
                            base::DictionaryValue* result) {
  using namespace onc::eap;
  using namespace onc::certificate;
  if (!ValidateObjectDefault(kEAPSignature, onc_object, result))
    return false;

  std::string inner;
  static const char* kValidInnerValues[] =
      { kAutomatic, kMD5, kMSCHAPv2, kPAP, NULL };
  if (result->GetStringWithoutPathExpansion(kInner, &inner) &&
      !RequireAnyOf(inner, kValidInnerValues)) {
    return false;
  }

  std::string outer;
  static const char* kValidOuterValues[] =
      { kPEAP, kEAP_TLS, kEAP_TTLS, kLEAP, kEAP_SIM, kEAP_FAST, kEAP_AKA,
        NULL };
  if (result->GetStringWithoutPathExpansion(kOuter, &outer) &&
      !RequireAnyOf(outer, kValidOuterValues)) {
    return false;
  }

  std::string cert_type;
  static const char* kValidCertTypes[] = { kRef, kPattern, NULL };
  if (result->GetStringWithoutPathExpansion(kClientCertType, &cert_type) &&
      !RequireAnyOf(cert_type, kValidCertTypes )) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, kOuter);

  if (cert_type == kPattern)
    allRequiredExist &= RequireField(*result, kClientCertPattern);
  else if (cert_type == kRef)
    allRequiredExist &= RequireField(*result, kClientCertRef);

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateCertificate(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  using namespace onc::certificate;
  if (!ValidateObjectDefault(kCertificateSignature, onc_object, result))
    return false;

  std::string type;
  static const char* kValidTypes[] = { kClient, kServer, kAuthority, NULL };
  if (result->GetStringWithoutPathExpansion(certificate::kType, &type) &&
      !RequireAnyOf(type, kValidTypes)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, kGUID);

  bool remove = false;
  result->GetBooleanWithoutPathExpansion(kRemove, &remove);
  if (!remove) {
    allRequiredExist &= RequireField(*result, certificate::kType);

    if (type == kClient)
      allRequiredExist &= RequireField(*result, kPKCS12);
    else if (type == kServer || type == kAuthority)
      allRequiredExist &= RequireField(*result, kX509);
  }

  return !error_on_missing_field_ || allRequiredExist;
}

}  // namespace onc
}  // namespace chromeos
