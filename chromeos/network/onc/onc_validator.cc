// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_validator.h"

#include <algorithm>
#include <string>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"

namespace chromeos {
namespace onc {

namespace {

std::string ValueToString(const base::Value& value) {
  std::string json;
  base::JSONWriter::Write(&value, &json);
  return json;
}

// Copied from policy/configuration_policy_handler.cc.
// TODO(pneubeck): move to a common place like base/.
std::string ValueTypeToString(Value::Type type) {
  static const char* strings[] = {
    "null",
    "boolean",
    "integer",
    "double",
    "string",
    "binary",
    "dictionary",
    "list"
  };
  CHECK(static_cast<size_t>(type) < arraysize(strings));
  return strings[type];
}

}  // namespace

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
    const base::DictionaryValue& onc_object,
    Result* result) {
  CHECK(object_signature != NULL);
  *result = VALID;
  error_or_warning_found_ = false;
  bool error = false;
  scoped_ptr<base::Value> result_value =
      MapValue(*object_signature, onc_object, &error);
  if (error) {
    *result = INVALID;
    result_value.reset();
  } else if (error_or_warning_found_) {
    *result = VALID_WITH_WARNINGS;
  }
  // The return value should be NULL if, and only if, |result| equals INVALID.
  DCHECK_EQ(result_value.get() == NULL, *result == INVALID);

  base::DictionaryValue* result_dict = NULL;
  if (result_value.get() != NULL) {
    result_value.release()->GetAsDictionary(&result_dict);
    CHECK(result_dict != NULL);
  }

  return make_scoped_ptr(result_dict);
}

scoped_ptr<base::Value> Validator::MapValue(
    const OncValueSignature& signature,
    const base::Value& onc_value,
    bool* error) {
  if (onc_value.GetType() != signature.onc_type) {
    LOG(ERROR) << ErrorHeader() << "Found value '" << onc_value
               << "' of type '" << ValueTypeToString(onc_value.GetType())
               << "', but type '" << ValueTypeToString(signature.onc_type)
               << "' is required.";
    error_or_warning_found_ = *error = true;
    return scoped_ptr<base::Value>();
  }

  scoped_ptr<base::Value> repaired =
      Mapper::MapValue(signature, onc_value, error);
  if (repaired.get() != NULL)
    CHECK_EQ(repaired->GetType(), signature.onc_type);
  return repaired.Pass();
}

scoped_ptr<base::DictionaryValue> Validator::MapObject(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object,
    bool* error) {
  scoped_ptr<base::DictionaryValue> repaired(new base::DictionaryValue);

  bool valid;
  if (&signature == &kToplevelConfigurationSignature)
    valid = ValidateToplevelConfiguration(onc_object, repaired.get());
  else if (&signature == &kNetworkConfigurationSignature)
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

  if (valid) {
    return repaired.Pass();
  } else {
    DCHECK(error_or_warning_found_);
    error_or_warning_found_ = *error = true;
    return scoped_ptr<base::DictionaryValue>();
  }
}

scoped_ptr<base::Value> Validator::MapField(
    const std::string& field_name,
    const OncValueSignature& object_signature,
    const base::Value& onc_value,
    bool* found_unknown_field,
    bool* error) {
  path_.push_back(field_name);
  bool current_field_unknown = false;
  scoped_ptr<base::Value> result = Mapper::MapField(
      field_name, object_signature, onc_value, &current_field_unknown, error);

  DCHECK_EQ(field_name, path_.back());
  path_.pop_back();

  if (current_field_unknown) {
    error_or_warning_found_ = *found_unknown_field = true;
    std::string message = MessageHeader(error_on_unknown_field_)
        + "Field name '" + field_name + "' is unknown.";
    if (error_on_unknown_field_)
      LOG(ERROR) << message;
    else
      LOG(WARNING) << message;
  }

  return result.Pass();
}

scoped_ptr<base::ListValue> Validator::MapArray(
    const OncValueSignature& array_signature,
    const base::ListValue& onc_array,
    bool* nested_error) {
  bool nested_error_in_current_array = false;
  scoped_ptr<base::ListValue> result = Mapper::MapArray(
      array_signature, onc_array, &nested_error_in_current_array);

  // Drop individual networks and certificates instead of rejecting all of
  // the configuration.
  if (nested_error_in_current_array &&
      &array_signature != &kNetworkConfigurationListSignature &&
      &array_signature != &kCertificateListSignature) {
    *nested_error = nested_error_in_current_array;
  }
  return result.Pass();
}

scoped_ptr<base::Value> Validator::MapEntry(int index,
                                            const OncValueSignature& signature,
                                            const base::Value& onc_value,
                                            bool* error) {
  std::string str = base::IntToString(index);
  path_.push_back(str);
  scoped_ptr<base::Value> result =
      Mapper::MapEntry(index, signature, onc_value, error);
  DCHECK_EQ(str, path_.back());
  path_.pop_back();
  return result.Pass();
}

bool Validator::ValidateObjectDefault(
    const OncValueSignature& signature,
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  bool found_unknown_field = false;
  bool nested_error_occured = false;
  MapFields(signature, onc_object, &found_unknown_field, &nested_error_occured,
            result);

  if (found_unknown_field && error_on_unknown_field_) {
    DVLOG(1) << "Unknown field names are errors: Aborting.";
    return false;
  }

  if (nested_error_occured)
    return false;

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
    error_or_warning_found_ = true;
    LOG(WARNING) << WarningHeader() << "Found the field '" << onc::kRecommended
                 << "' in an unmanaged ONC. Removing it.";
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
      error_or_warning_found_ = true;
      path_.push_back(onc::kRecommended);
      std::string message = MessageHeader(error_on_wrong_recommended_) +
          "The " + error_cause + " field '" + field_name +
          "' cannot be recommended.";
      path_.pop_back();
      if (error_on_wrong_recommended_) {
        LOG(ERROR) << message;
        return false;
      } else {
        LOG(WARNING) << message;
        continue;
      }
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

}  // namespace

bool Validator::FieldExistsAndHasNoValidValue(
    const base::DictionaryValue& object,
    const std::string &field_name,
    const char** valid_values) {
  std::string actual_value;
  if (!object.GetStringWithoutPathExpansion(field_name, &actual_value))
    return false;

  const char** it = valid_values;
  for (; *it != NULL; ++it) {
    if (actual_value == *it)
      return false;
  }
  error_or_warning_found_ = true;
  std::string valid_values_str =
      "[" + JoinStringRange(valid_values, it, ", ") + "]";
  path_.push_back(field_name);
  LOG(ERROR) << ErrorHeader() << "Found value '" << actual_value <<
      "', but expected one of the values " << valid_values_str;
  path_.pop_back();
  return true;
}

bool Validator::FieldExistsAndIsNotInRange(const base::DictionaryValue& object,
                                           const std::string &field_name,
                                           int lower_bound,
                                           int upper_bound) {
  int actual_value;
  if (!object.GetIntegerWithoutPathExpansion(field_name, &actual_value) ||
      (lower_bound <= actual_value && actual_value <= upper_bound)) {
    return false;
  }
  error_or_warning_found_ = true;
  path_.push_back(field_name);
  LOG(ERROR) << ErrorHeader() << "Found value '" << actual_value
             << "', but expected a value in the range [" << lower_bound
             << ", " << upper_bound << "] (boundaries inclusive)";
  path_.pop_back();
  return true;
}

bool Validator::RequireField(const base::DictionaryValue& dict,
                             const std::string& field_name) {
  if (dict.HasKey(field_name))
    return true;
  error_or_warning_found_ = true;
  LOG(ERROR) << ErrorHeader() << "The required field '" << field_name
             << "' is missing.";
  return false;
}

// Prohibit certificate patterns for device policy ONC so that an unmanaged user
// won't have a certificate presented for them involuntarily.
bool Validator::CertPatternInDevicePolicy(const std::string& cert_type) {
  if (cert_type == certificate::kPattern &&
      onc_source_ == ONC_SOURCE_DEVICE_POLICY) {
    error_or_warning_found_ = true;
    LOG(ERROR) << ErrorHeader() << "Client certificate patterns are "
               << "prohibited in ONC device policies.";
    return true;
  }
  return false;
}

bool Validator::ValidateToplevelConfiguration(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  if (!ValidateObjectDefault(kToplevelConfigurationSignature,
                             onc_object, result)) {
    return false;
  }

  static const char* kValidTypes[] =
      { kUnencryptedConfiguration, kEncryptedConfiguration, NULL };
  if (FieldExistsAndHasNoValidValue(*result, kType, kValidTypes))
    return false;

  bool allRequiredExist = true;

  // Not part of the ONC spec yet:
  // We don't require the type field and default to UnencryptedConfiguration.
  std::string type = kUnencryptedConfiguration;
  result->GetStringWithoutPathExpansion(kType, &type);
  if (type == kUnencryptedConfiguration &&
      !result->HasKey(kNetworkConfigurations) &&
      !result->HasKey(kCertificates)) {
    error_or_warning_found_ = true;
    std::string message = MessageHeader(error_on_missing_field_) +
        "Neither the field '" + kNetworkConfigurations + "' nor '" +
        kCertificates + "is present, but at least one is required.";
    if (error_on_missing_field_)
      LOG(ERROR) << message;
    else
      LOG(WARNING) << message;
    allRequiredExist = false;
  }

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateNetworkConfiguration(
    const base::DictionaryValue& onc_object,
    base::DictionaryValue* result) {
  if (!ValidateObjectDefault(kNetworkConfigurationSignature,
                             onc_object, result)) {
    return false;
  }

  static const char* kValidTypes[] = { kEthernet, kVPN, kWiFi, NULL };
  if (FieldExistsAndHasNoValidValue(*result, kType, kValidTypes))
    return false;

  bool allRequiredExist = RequireField(*result, kGUID);

  bool remove = false;
  result->GetBooleanWithoutPathExpansion(kRemove, &remove);
  if (!remove) {
    allRequiredExist &= RequireField(*result, kName);
    allRequiredExist &= RequireField(*result, kType);

    std::string type;
    result->GetStringWithoutPathExpansion(kType, &type);

    // Prohibit anything but WiFi and Ethernet for device-level policy (which
    // corresponds to shared networks). See also http://crosbug.com/28741.
    if (onc_source_ == ONC_SOURCE_DEVICE_POLICY &&
        type != kWiFi &&
        type != kEthernet) {
      error_or_warning_found_ = true;
      LOG(ERROR) << ErrorHeader() << "Networks of type '"
                 << type << "' are prohibited in ONC device policies.";
      return false;
    }
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

  static const char* kValidAuthentications[] = { kNone, k8021X, NULL };
  if (FieldExistsAndHasNoValidValue(*result, kAuthentication,
                                    kValidAuthentications)) {
    return false;
  }

  bool allRequiredExist = true;
  std::string auth;
  result->GetStringWithoutPathExpansion(kAuthentication, &auth);
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

  static const char* kValidTypes[] = { kIPv4, kIPv6, NULL };
  if (FieldExistsAndHasNoValidValue(*result, ipconfig::kType, kValidTypes))
    return false;

  std::string type;
  result->GetStringWithoutPathExpansion(ipconfig::kType, &type);
  int lower_bound = 1;
  // In case of missing type, choose higher upper_bound.
  int upper_bound = (type == kIPv4) ? 32 : 128;
  if (FieldExistsAndIsNotInRange(*result, kRoutingPrefix,
                                 lower_bound, upper_bound)) {
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

  static const char* kValidSecurities[] =
      { kNone, kWEP_PSK, kWEP_8021X, kWPA_PSK, kWPA_EAP, NULL };
  if (FieldExistsAndHasNoValidValue(*result, kSecurity, kValidSecurities))
    return false;

  bool allRequiredExist = RequireField(*result, kSecurity) &
      RequireField(*result, kSSID);

  std::string security;
  result->GetStringWithoutPathExpansion(kSecurity, &security);
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

  static const char* kValidTypes[] =
      { kIPsec, kTypeL2TP_IPsec, kOpenVPN, NULL };
  if (FieldExistsAndHasNoValidValue(*result, vpn::kType, kValidTypes))
    return false;

  bool allRequiredExist = RequireField(*result, vpn::kType);
  std::string type;
  result->GetStringWithoutPathExpansion(vpn::kType, &type);
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

  static const char* kValidAuthentications[] = { kPSK, kCert, NULL };
  static const char* kValidCertTypes[] = { kRef, kPattern, NULL };
  // Using strict bit-wise OR to check all conditions.
  if (FieldExistsAndHasNoValidValue(*result, kAuthenticationType,
                                    kValidAuthentications) |
      FieldExistsAndHasNoValidValue(*result, kClientCertType,
                                    kValidCertTypes)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, kAuthenticationType) &
      RequireField(*result, kIKEVersion);
  std::string auth;
  result->GetStringWithoutPathExpansion(kAuthenticationType, &auth);
  if (auth == kCert) {
    allRequiredExist &= RequireField(*result, kClientCertType) &
        RequireField(*result, kServerCARef);
  }
  std::string cert_type;
  result->GetStringWithoutPathExpansion(kClientCertType, &cert_type);

  if (CertPatternInDevicePolicy(cert_type))
    return false;

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

  static const char* kValidAuthRetryValues[] =
      { openvpn::kNone, kInteract, kNoInteract, NULL };
  static const char* kValidCertTypes[] =
      { certificate::kNone, kRef, kPattern, NULL };
  static const char* kValidCertTlsValues[] =
      { openvpn::kNone, openvpn::kServer, NULL };

  // Using strict bit-wise OR to check all conditions.
  if (FieldExistsAndHasNoValidValue(*result, kAuthRetry,
                                    kValidAuthRetryValues) |
      FieldExistsAndHasNoValidValue(*result, kClientCertType, kValidCertTypes) |
      FieldExistsAndHasNoValidValue(*result, kRemoteCertTLS,
                                    kValidCertTlsValues)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, kClientCertType);
  std::string cert_type;
  result->GetStringWithoutPathExpansion(kClientCertType, &cert_type);

  if (CertPatternInDevicePolicy(cert_type))
    return false;

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
    error_or_warning_found_ = true;
    allRequiredExist = false;
    std::string message = MessageHeader(error_on_missing_field_) +
        "None of the fields '" + kSubject + "', '" + kIssuer + "', and '" +
        kIssuerCARef + "' is present, but at least one is required.";
    if (error_on_missing_field_)
      LOG(ERROR) << message;
    else
      LOG(WARNING) << message;
  }

  return !error_on_missing_field_ || allRequiredExist;
}

bool Validator::ValidateProxySettings(const base::DictionaryValue& onc_object,
                                      base::DictionaryValue* result) {
  using namespace onc::proxy;
  if (!ValidateObjectDefault(kProxySettingsSignature, onc_object, result))
    return false;

  static const char* kValidTypes[] = { kDirect, kManual, kPAC, kWPAD, NULL };
  if (FieldExistsAndHasNoValidValue(*result, proxy::kType, kValidTypes))
    return false;

  bool allRequiredExist = RequireField(*result, proxy::kType);
  std::string type;
  result->GetStringWithoutPathExpansion(proxy::kType, &type);
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

  static const char* kValidInnerValues[] =
      { kAutomatic, kMD5, kMSCHAPv2, kPAP, NULL };
  static const char* kValidOuterValues[] =
      { kPEAP, kEAP_TLS, kEAP_TTLS, kLEAP, kEAP_SIM, kEAP_FAST, kEAP_AKA,
        NULL };
  static const char* kValidCertTypes[] = { kRef, kPattern, NULL };

  // Using strict bit-wise OR to check all conditions.
  if (FieldExistsAndHasNoValidValue(*result, kInner, kValidInnerValues) |
      FieldExistsAndHasNoValidValue(*result, kOuter, kValidOuterValues) |
      FieldExistsAndHasNoValidValue(*result, kClientCertType,
                                    kValidCertTypes)) {
    return false;
  }

  bool allRequiredExist = RequireField(*result, kOuter);
  std::string cert_type;
  result->GetStringWithoutPathExpansion(kClientCertType, &cert_type);

  if (CertPatternInDevicePolicy(cert_type))
    return false;

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

  static const char* kValidTypes[] = { kClient, kServer, kAuthority, NULL };
  if (FieldExistsAndHasNoValidValue(*result, certificate::kType, kValidTypes))
    return false;

  bool allRequiredExist = RequireField(*result, kGUID);

  bool remove = false;
  result->GetBooleanWithoutPathExpansion(kRemove, &remove);
  if (!remove) {
    allRequiredExist &= RequireField(*result, certificate::kType);

    std::string type;
    result->GetStringWithoutPathExpansion(certificate::kType, &type);
    if (type == kClient)
      allRequiredExist &= RequireField(*result, kPKCS12);
    else if (type == kServer || type == kAuthority)
      allRequiredExist &= RequireField(*result, kX509);
  }

  return !error_on_missing_field_ || allRequiredExist;
}

std::string Validator::WarningHeader() {
  return MessageHeader(false);
}

std::string Validator::ErrorHeader() {
  return MessageHeader(true);
}

std::string Validator::MessageHeader(bool is_error) {
  std::string path = path_.empty() ? "toplevel" : JoinString(path_, ".");
  std::string message = "At " + path + ": ";
  return message;
}

}  // namespace onc
}  // namespace chromeos
