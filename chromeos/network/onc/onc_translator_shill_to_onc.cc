// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_translator.h"

#include <string>

#include "base/basictypes.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace onc {

namespace {

// Converts |str| to a base::Value of the given |type|. If the conversion fails,
// returns NULL.
scoped_ptr<base::Value> ConvertStringToValue(const std::string& str,
                                             base::Value::Type type) {
  base::Value* value;
  if (type == base::Value::TYPE_STRING) {
    value = new base::StringValue(str);
  } else {
    value = base::JSONReader::Read(str);
  }

  if (value == NULL || value->GetType() != type) {
    delete value;
    value = NULL;
  }
  return make_scoped_ptr(value);
}

// This class implements the translation of properties from the given
// |shill_dictionary| to a new ONC object of signature |onc_signature|. Using
// recursive calls to CreateTranslatedONCObject of new instances, nested objects
// are translated.
class ShillToONCTranslator {
 public:
  ShillToONCTranslator(const base::DictionaryValue& shill_dictionary,
                       const OncValueSignature& onc_signature)
      : shill_dictionary_(&shill_dictionary),
        onc_signature_(&onc_signature) {
    field_translation_table_ = GetFieldTranslationTable(onc_signature);
  }

  // Translates the associated Shill dictionary and creates an ONC object of the
  // given signature.
  scoped_ptr<base::DictionaryValue> CreateTranslatedONCObject();

 private:
  void TranslateEthernet();
  void TranslateOpenVPN();
  void TranslateIPsec();
  void TranslateVPN();
  void TranslateWiFiWithState();
  void TranslateCellularWithState();
  void TranslateNetworkWithState();
  void TranslateIPConfig();

  // Creates an ONC object from |dictionary| according to the signature
  // associated to |onc_field_name| and adds it to |onc_object_| at
  // |onc_field_name|.
  void TranslateAndAddNestedObject(const std::string& onc_field_name,
                                   const base::DictionaryValue& dictionary);

  // Creates an ONC object from |shill_dictionary_| according to the signature
  // associated to |onc_field_name| and adds it to |onc_object_| at
  // |onc_field_name|.
  void TranslateAndAddNestedObject(const std::string& onc_field_name);

  // Translates a list of nested objects and adds the list to |onc_object_| at
  // |onc_field_name|. If there are errors while parsing individual objects or
  // if the resulting list contains no entries, the result will not be added to
  // |onc_object_|.
  void TranslateAndAddListOfObjects(const std::string& onc_field_name,
                                    const base::ListValue& list);

  // Applies function CopyProperty to each field of |value_signature| and its
  // base signatures.
  void CopyPropertiesAccordingToSignature(
      const OncValueSignature* value_signature);

  // Applies function CopyProperty to each field of |onc_signature_| and its
  // base signatures.
  void CopyPropertiesAccordingToSignature();

  // If |shill_property_name| is defined in |field_signature|, copies this
  // entry from |shill_dictionary_| to |onc_object_| if it exists.
  void CopyProperty(const OncFieldSignature* field_signature);

  // If existent, translates the entry at |shill_property_name| in
  // |shill_dictionary_| using |table|. It is an error if no matching table
  // entry is found. Writes the result as entry at |onc_field_name| in
  // |onc_object_|.
  void TranslateWithTableAndSet(const std::string& shill_property_name,
                                const StringTranslationEntry table[],
                                const std::string& onc_field_name);

  const base::DictionaryValue* shill_dictionary_;
  const OncValueSignature* onc_signature_;
  const FieldTranslationEntry* field_translation_table_;
  scoped_ptr<base::DictionaryValue> onc_object_;

  DISALLOW_COPY_AND_ASSIGN(ShillToONCTranslator);
};

scoped_ptr<base::DictionaryValue>
ShillToONCTranslator::CreateTranslatedONCObject() {
  onc_object_.reset(new base::DictionaryValue);
  if (onc_signature_ == &kNetworkWithStateSignature) {
    TranslateNetworkWithState();
  } else if (onc_signature_ == &kEthernetSignature) {
    TranslateEthernet();
  } else if (onc_signature_ == &kVPNSignature) {
    TranslateVPN();
  } else if (onc_signature_ == &kOpenVPNSignature) {
    TranslateOpenVPN();
  } else if (onc_signature_ == &kIPsecSignature) {
    TranslateIPsec();
  } else if (onc_signature_ == &kWiFiWithStateSignature) {
    TranslateWiFiWithState();
  } else if (onc_signature_ == &kCellularWithStateSignature) {
    TranslateCellularWithState();
  } else if (onc_signature_ == &kIPConfigSignature) {
    TranslateIPConfig();
  } else {
    CopyPropertiesAccordingToSignature();
  }
  return onc_object_.Pass();
}

void ShillToONCTranslator::TranslateEthernet() {
  std::string shill_network_type;
  shill_dictionary_->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                   &shill_network_type);
  const char* onc_auth = ::onc::ethernet::kAuthenticationNone;
  if (shill_network_type == shill::kTypeEthernetEap)
    onc_auth = ::onc::ethernet::k8021X;
  onc_object_->SetStringWithoutPathExpansion(::onc::ethernet::kAuthentication,
                                             onc_auth);
}

void ShillToONCTranslator::TranslateOpenVPN() {
  if (shill_dictionary_->HasKey(shill::kOpenVPNVerifyX509NameProperty))
    TranslateAndAddNestedObject(::onc::openvpn::kVerifyX509);

  // Shill supports only one RemoteCertKU but ONC requires a list. If existing,
  // wraps the value into a list.
  std::string certKU;
  if (shill_dictionary_->GetStringWithoutPathExpansion(
          shill::kOpenVPNRemoteCertKUProperty, &certKU)) {
    scoped_ptr<base::ListValue> certKUs(new base::ListValue);
    certKUs->AppendString(certKU);
    onc_object_->SetWithoutPathExpansion(::onc::openvpn::kRemoteCertKU,
                                         certKUs.release());
  }

  for (const OncFieldSignature* field_signature = onc_signature_->fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    const std::string& onc_field_name = field_signature->onc_field_name;
    if (onc_field_name == ::onc::vpn::kSaveCredentials ||
        onc_field_name == ::onc::openvpn::kRemoteCertKU ||
        onc_field_name == ::onc::openvpn::kServerCAPEMs) {
      CopyProperty(field_signature);
      continue;
    }

    std::string shill_property_name;
    const base::Value* shill_value = NULL;
    if (!field_translation_table_ ||
        !GetShillPropertyName(field_signature->onc_field_name,
                              field_translation_table_,
                              &shill_property_name) ||
        !shill_dictionary_->GetWithoutPathExpansion(shill_property_name,
                                                    &shill_value)) {
      continue;
    }

    scoped_ptr<base::Value> translated;
    std::string shill_str;
    if (shill_value->GetAsString(&shill_str)) {
      // Shill wants all Provider/VPN fields to be strings. Translates these
      // strings back to the correct ONC type.
      translated = ConvertStringToValue(
          shill_str,
          field_signature->value_signature->onc_type);

      if (translated.get() == NULL) {
        LOG(ERROR) << "Shill property '" << shill_property_name
                   << "' with value " << *shill_value
                   << " couldn't be converted to base::Value::Type "
                   << field_signature->value_signature->onc_type;
      } else {
        onc_object_->SetWithoutPathExpansion(onc_field_name,
                                             translated.release());
      }
    } else {
      LOG(ERROR) << "Shill property '" << shill_property_name
                 << "' has value " << *shill_value
                 << ", but expected a string";
    }
  }
}

void ShillToONCTranslator::TranslateIPsec() {
  CopyPropertiesAccordingToSignature();
  if (shill_dictionary_->HasKey(shill::kL2tpIpsecXauthUserProperty))
    TranslateAndAddNestedObject(::onc::ipsec::kXAUTH);
}

void ShillToONCTranslator::TranslateVPN() {
  TranslateWithTableAndSet(
      shill::kProviderTypeProperty, kVPNTypeTable, ::onc::vpn::kType);
  CopyPropertiesAccordingToSignature();

  std::string vpn_type;
  if (onc_object_->GetStringWithoutPathExpansion(::onc::vpn::kType,
                                                 &vpn_type)) {
    if (vpn_type == ::onc::vpn::kTypeL2TP_IPsec) {
      TranslateAndAddNestedObject(::onc::vpn::kIPsec);
      TranslateAndAddNestedObject(::onc::vpn::kL2TP);
    } else {
      TranslateAndAddNestedObject(vpn_type);
    }
  }
}

void ShillToONCTranslator::TranslateWiFiWithState() {
  TranslateWithTableAndSet(
      shill::kSecurityProperty, kWiFiSecurityTable, ::onc::wifi::kSecurity);
  std::string ssid = shill_property_util::GetSSIDFromProperties(
      *shill_dictionary_, NULL /* ignore unknown encoding */);
  if (!ssid.empty())
    onc_object_->SetStringWithoutPathExpansion(::onc::wifi::kSSID, ssid);
  CopyPropertiesAccordingToSignature();
}

void ShillToONCTranslator::TranslateCellularWithState() {
  CopyPropertiesAccordingToSignature();
  const base::DictionaryValue* dictionary = NULL;
  if (shill_dictionary_->GetDictionaryWithoutPathExpansion(
        shill::kServingOperatorProperty, &dictionary)) {
    TranslateAndAddNestedObject(::onc::cellular::kServingOperator, *dictionary);
  }
  if (shill_dictionary_->GetDictionaryWithoutPathExpansion(
        shill::kCellularApnProperty, &dictionary)) {
    TranslateAndAddNestedObject(::onc::cellular::kAPN, *dictionary);
  }
  const base::ListValue* shill_apns = NULL;
  if (shill_dictionary_->GetListWithoutPathExpansion(
          shill::kCellularApnListProperty, &shill_apns)) {
    TranslateAndAddListOfObjects(::onc::cellular::kAPNList, *shill_apns);
  }

  const base::DictionaryValue* device_dictionary = NULL;
  if (!shill_dictionary_->GetDictionaryWithoutPathExpansion(
          shill::kDeviceProperty, &device_dictionary)) {
    return;
  }

  // Iterate through all fields of the CellularWithState signature and copy
  // values from the device properties according to the separate
  // CellularDeviceTable.
  for (const OncFieldSignature* field_signature = onc_signature_->fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    const std::string& onc_field_name = field_signature->onc_field_name;

    std::string shill_property_name;
    const base::Value* shill_value = NULL;
    if (!GetShillPropertyName(field_signature->onc_field_name,
                              kCellularDeviceTable,
                              &shill_property_name) ||
        !device_dictionary->GetWithoutPathExpansion(shill_property_name,
                                                    &shill_value)) {
      continue;
    }
    onc_object_->SetWithoutPathExpansion(onc_field_name,
                                         shill_value->DeepCopy());
  }
}

void ShillToONCTranslator::TranslateNetworkWithState() {
  CopyPropertiesAccordingToSignature();

  std::string shill_network_type;
  shill_dictionary_->GetStringWithoutPathExpansion(shill::kTypeProperty,
                                                   &shill_network_type);
  std::string onc_network_type = ::onc::network_type::kEthernet;
  if (shill_network_type != shill::kTypeEthernet &&
      shill_network_type != shill::kTypeEthernetEap) {
    TranslateStringToONC(
        kNetworkTypeTable, shill_network_type, &onc_network_type);
  }
  // Translate nested Cellular, WiFi, etc. properties.
  if (!onc_network_type.empty()) {
    onc_object_->SetStringWithoutPathExpansion(::onc::network_config::kType,
                                               onc_network_type);
    TranslateAndAddNestedObject(onc_network_type);
  }

  // Since Name is a read only field in Shill unless it's a VPN, it is copied
  // here, but not when going the other direction (if it's not a VPN).
  std::string name;
  shill_dictionary_->GetStringWithoutPathExpansion(shill::kNameProperty,
                                                   &name);
  onc_object_->SetStringWithoutPathExpansion(::onc::network_config::kName,
                                             name);

  // Limit ONC state to "NotConnected", "Connected", or "Connecting".
  std::string state;
  if (shill_dictionary_->GetStringWithoutPathExpansion(shill::kStateProperty,
                                                       &state)) {
    std::string onc_state = ::onc::connection_state::kNotConnected;
    if (NetworkState::StateIsConnected(state)) {
      onc_state = ::onc::connection_state::kConnected;
    } else if (NetworkState::StateIsConnecting(state)) {
      onc_state = ::onc::connection_state::kConnecting;
    }
    onc_object_->SetStringWithoutPathExpansion(
        ::onc::network_config::kConnectionState, onc_state);
  }

  // Use a human-readable aa:bb format for any hardware MAC address. Note:
  // this property is provided by the caller but is not part of the Shill
  // Service properties (it is copied from the Device properties).
  std::string address;
  if (shill_dictionary_->GetStringWithoutPathExpansion(shill::kAddressProperty,
                                                       &address)) {
    onc_object_->SetStringWithoutPathExpansion(
        ::onc::network_config::kMacAddress,
        network_util::FormattedMacAddress(address));
  }

  // Shill's Service has an IPConfig property (note the singular), not an
  // IPConfigs property. However, we require the caller of the translation to
  // patch the Shill dictionary before passing it to the translator.
  const base::ListValue* shill_ipconfigs = NULL;
  if (shill_dictionary_->GetListWithoutPathExpansion(shill::kIPConfigsProperty,
                                                     &shill_ipconfigs)) {
    TranslateAndAddListOfObjects(::onc::network_config::kIPConfigs,
                                 *shill_ipconfigs);
  }
}

void ShillToONCTranslator::TranslateIPConfig() {
  CopyPropertiesAccordingToSignature();
  std::string shill_ip_method;
  shill_dictionary_->GetStringWithoutPathExpansion(shill::kMethodProperty,
                                                   &shill_ip_method);
  std::string type;
  if (shill_ip_method == shill::kTypeIPv4 ||
      shill_ip_method == shill::kTypeDHCP) {
    type = ::onc::ipconfig::kIPv4;
  } else if (shill_ip_method == shill::kTypeIPv6 ||
             shill_ip_method == shill::kTypeDHCP6) {
    type = ::onc::ipconfig::kIPv6;
  } else {
    return;  // Ignore unhandled IPConfig types, e.g. bootp, zeroconf, ppp
  }

  onc_object_->SetStringWithoutPathExpansion(::onc::ipconfig::kType, type);
}

void ShillToONCTranslator::TranslateAndAddNestedObject(
    const std::string& onc_field_name) {
  TranslateAndAddNestedObject(onc_field_name, *shill_dictionary_);
}

void ShillToONCTranslator::TranslateAndAddNestedObject(
    const std::string& onc_field_name,
    const base::DictionaryValue& dictionary) {
  const OncFieldSignature* field_signature =
      GetFieldSignature(*onc_signature_, onc_field_name);
  DCHECK(field_signature) << "Unable to find signature for field "
                          << onc_field_name << ".";
  ShillToONCTranslator nested_translator(dictionary,
                                         *field_signature->value_signature);
  scoped_ptr<base::DictionaryValue> nested_object =
      nested_translator.CreateTranslatedONCObject();
  if (nested_object->empty())
    return;
  onc_object_->SetWithoutPathExpansion(onc_field_name, nested_object.release());
}

void ShillToONCTranslator::TranslateAndAddListOfObjects(
    const std::string& onc_field_name,
    const base::ListValue& list) {
  const OncFieldSignature* field_signature =
      GetFieldSignature(*onc_signature_, onc_field_name);
  if (field_signature->value_signature->onc_type != base::Value::TYPE_LIST) {
    LOG(ERROR) << "ONC Field name: '" << onc_field_name << "' has type '"
               << field_signature->value_signature->onc_type
               << "', expected: base::Value::TYPE_LIST.";
    return;
  }
  DCHECK(field_signature->value_signature->onc_array_entry_signature);
  scoped_ptr<base::ListValue> result(new base::ListValue());
  for (base::ListValue::const_iterator it = list.begin();
       it != list.end(); ++it) {
    const base::DictionaryValue* shill_value = NULL;
    if (!(*it)->GetAsDictionary(&shill_value))
      continue;
    ShillToONCTranslator nested_translator(
        *shill_value,
        *field_signature->value_signature->onc_array_entry_signature);
    scoped_ptr<base::DictionaryValue> nested_object =
        nested_translator.CreateTranslatedONCObject();
    // If the nested object couldn't be parsed, simply omit it.
    if (nested_object->empty())
      continue;
    result->Append(nested_object.release());
  }
  // If there are no entries in the list, there is no need to expose this field.
  if (result->empty())
    return;
  onc_object_->SetWithoutPathExpansion(onc_field_name, result.release());
}

void ShillToONCTranslator::CopyPropertiesAccordingToSignature() {
  CopyPropertiesAccordingToSignature(onc_signature_);
}

void ShillToONCTranslator::CopyPropertiesAccordingToSignature(
    const OncValueSignature* value_signature) {
  if (value_signature->base_signature)
    CopyPropertiesAccordingToSignature(value_signature->base_signature);
  for (const OncFieldSignature* field_signature = value_signature->fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    CopyProperty(field_signature);
  }
}

void ShillToONCTranslator::CopyProperty(
    const OncFieldSignature* field_signature) {
  std::string shill_property_name;
  const base::Value* shill_value = NULL;
  if (!field_translation_table_ ||
      !GetShillPropertyName(field_signature->onc_field_name,
                            field_translation_table_,
                            &shill_property_name) ||
      !shill_dictionary_->GetWithoutPathExpansion(shill_property_name,
                                                  &shill_value)) {
    return;
  }

  if (shill_value->GetType() != field_signature->value_signature->onc_type) {
    LOG(ERROR) << "Shill property '" << shill_property_name
               << "' with value " << *shill_value
               << " has base::Value::Type " << shill_value->GetType()
               << " but ONC field '" << field_signature->onc_field_name
               << "' requires type "
               << field_signature->value_signature->onc_type << ".";
    return;
  }

 onc_object_->SetWithoutPathExpansion(field_signature->onc_field_name,
                                      shill_value->DeepCopy());
}

void ShillToONCTranslator::TranslateWithTableAndSet(
    const std::string& shill_property_name,
    const StringTranslationEntry table[],
    const std::string& onc_field_name) {
  std::string shill_value;
  if (!shill_dictionary_->GetStringWithoutPathExpansion(shill_property_name,
                                                        &shill_value)) {
    return;
  }
  std::string onc_value;
  if (TranslateStringToONC(table, shill_value, &onc_value)) {
    onc_object_->SetStringWithoutPathExpansion(onc_field_name, onc_value);
    return;
  }
  LOG(ERROR) << "Shill property '" << shill_property_name << "' with value "
             << shill_value << " couldn't be translated to ONC";
}

}  // namespace

scoped_ptr<base::DictionaryValue> TranslateShillServiceToONCPart(
    const base::DictionaryValue& shill_dictionary,
    const OncValueSignature* onc_signature) {
  CHECK(onc_signature != NULL);

  ShillToONCTranslator translator(shill_dictionary, *onc_signature);
  return translator.CreateTranslatedONCObject();
}

}  // namespace onc
}  // namespace chromeos
