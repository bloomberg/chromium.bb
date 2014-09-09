// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The implementation of TranslateONCObjectToShill is structured in two parts:
// - The recursion through the existing ONC hierarchy
//     see TranslateONCHierarchy
// - The local translation of an object depending on the associated signature
//     see LocalTranslator::TranslateFields

#include "chromeos/network/onc/onc_translator.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "chromeos/network/shill_property_util.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace onc {

namespace {

bool ConvertListValueToStringVector(const base::ListValue& string_list,
                                    std::vector<std::string>* result) {
  for (size_t i = 0; i < string_list.GetSize(); ++i) {
    std::string str;
    if (!string_list.GetString(i, &str))
      return false;
    result->push_back(str);
  }
  return true;
}

scoped_ptr<base::StringValue> ConvertValueToString(const base::Value& value) {
  std::string str;
  if (!value.GetAsString(&str))
    base::JSONWriter::Write(&value, &str);
  return make_scoped_ptr(new base::StringValue(str));
}

// This class is responsible to translate the local fields of the given
// |onc_object| according to |onc_signature| into |shill_dictionary|. This
// translation should consider (if possible) only fields of this ONC object and
// not nested objects because recursion is handled by the calling function
// TranslateONCHierarchy.
class LocalTranslator {
 public:
  LocalTranslator(const OncValueSignature& onc_signature,
                  const base::DictionaryValue& onc_object,
                  base::DictionaryValue* shill_dictionary)
      : onc_signature_(&onc_signature),
        onc_object_(&onc_object),
        shill_dictionary_(shill_dictionary) {
    field_translation_table_ = GetFieldTranslationTable(onc_signature);
  }

  void TranslateFields();

 private:
  void TranslateEthernet();
  void TranslateOpenVPN();
  void TranslateIPsec();
  void TranslateVPN();
  void TranslateWiFi();
  void TranslateEAP();
  void TranslateStaticIPConfig();
  void TranslateNetworkConfiguration();

  // Copies all entries from |onc_object_| to |shill_dictionary_| for which a
  // translation (shill_property_name) is defined by |onc_signature_|.
  void CopyFieldsAccordingToSignature();

  // Adds |value| to |shill_dictionary| at the field shill_property_name given
  // by the associated signature. Takes ownership of |value|. Does nothing if
  // |value| is NULL or the property name cannot be read from the signature.
  void AddValueAccordingToSignature(const std::string& onc_field_name,
                                    scoped_ptr<base::Value> value);

  // If existent, translates the entry at |onc_field_name| in |onc_object_|
  // using |table|. It is an error if no matching table entry is found. Writes
  // the result as entry at |shill_property_name| in |shill_dictionary_|.
  void TranslateWithTableAndSet(const std::string& onc_field_name,
                                const StringTranslationEntry table[],
                                const std::string& shill_property_name);

  const OncValueSignature* onc_signature_;
  const FieldTranslationEntry* field_translation_table_;
  const base::DictionaryValue* onc_object_;
  base::DictionaryValue* shill_dictionary_;

  DISALLOW_COPY_AND_ASSIGN(LocalTranslator);
};

void LocalTranslator::TranslateFields() {
  if (onc_signature_ == &kNetworkConfigurationSignature)
    TranslateNetworkConfiguration();
  else if (onc_signature_ == &kStaticIPConfigSignature)
    TranslateStaticIPConfig();
  else if (onc_signature_ == &kEthernetSignature)
    TranslateEthernet();
  else if (onc_signature_ == &kVPNSignature)
    TranslateVPN();
  else if (onc_signature_ == &kOpenVPNSignature)
    TranslateOpenVPN();
  else if (onc_signature_ == &kIPsecSignature)
    TranslateIPsec();
  else if (onc_signature_ == &kWiFiSignature)
    TranslateWiFi();
  else if (onc_signature_ == &kEAPSignature)
    TranslateEAP();
  else
    CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateEthernet() {
  std::string authentication;
  onc_object_->GetStringWithoutPathExpansion(::onc::ethernet::kAuthentication,
                                             &authentication);

  const char* shill_type = shill::kTypeEthernet;
  if (authentication == ::onc::ethernet::k8021X)
    shill_type = shill::kTypeEthernetEap;
  shill_dictionary_->SetStringWithoutPathExpansion(shill::kTypeProperty,
                                                   shill_type);

  CopyFieldsAccordingToSignature();
}


void LocalTranslator::TranslateStaticIPConfig() {
  const base::ListValue* onc_nameservers = NULL;
  if (onc_object_->GetListWithoutPathExpansion(::onc::ipconfig::kNameServers,
                                               &onc_nameservers)) {
    std::vector<std::string> onc_nameservers_vector;
    ConvertListValueToStringVector(*onc_nameservers, &onc_nameservers_vector);
    std::string shill_nameservers = JoinString(onc_nameservers_vector, ',');
    shill_dictionary_->SetStringWithoutPathExpansion(
        shill::kStaticIPNameServersProperty, shill_nameservers);
  }

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateOpenVPN() {
  // SaveCredentials needs special handling when translating from Shill -> ONC
  // so handle it explicitly here.
  bool save_credentials;
  if (onc_object_->GetBooleanWithoutPathExpansion(
          ::onc::vpn::kSaveCredentials, &save_credentials)) {
    shill_dictionary_->SetBooleanWithoutPathExpansion(
        shill::kSaveCredentialsProperty, save_credentials);
  }
  // Shill supports only one RemoteCertKU but ONC a list.
  // Copy only the first entry if existing.
  const base::ListValue* certKUs = NULL;
  std::string certKU;
  if (onc_object_->GetListWithoutPathExpansion(::onc::openvpn::kRemoteCertKU,
                                               &certKUs) &&
      certKUs->GetString(0, &certKU)) {
    shill_dictionary_->SetStringWithoutPathExpansion(
        shill::kOpenVPNRemoteCertKUProperty, certKU);
  }

  for (base::DictionaryValue::Iterator it(*onc_object_); !it.IsAtEnd();
       it.Advance()) {
    scoped_ptr<base::Value> translated;
    if (it.key() == ::onc::openvpn::kRemoteCertKU ||
        it.key() == ::onc::openvpn::kServerCAPEMs) {
      translated.reset(it.value().DeepCopy());
    } else {
      // Shill wants all Provider/VPN fields to be strings.
      translated = ConvertValueToString(it.value());
    }
    AddValueAccordingToSignature(it.key(), translated.Pass());
  }
}

void LocalTranslator::TranslateIPsec() {
  CopyFieldsAccordingToSignature();

  // SaveCredentials needs special handling when translating from Shill -> ONC
  // so handle it explicitly here.
  bool save_credentials;
  if (onc_object_->GetBooleanWithoutPathExpansion(
          ::onc::vpn::kSaveCredentials, &save_credentials)) {
    shill_dictionary_->SetBooleanWithoutPathExpansion(
        shill::kSaveCredentialsProperty, save_credentials);
  }
}

void LocalTranslator::TranslateVPN() {
  std::string host;
  if (onc_object_->GetStringWithoutPathExpansion(::onc::vpn::kHost, &host)) {
    shill_dictionary_->SetStringWithoutPathExpansion(
        shill::kProviderHostProperty, host);
  }
  std::string type;
  onc_object_->GetStringWithoutPathExpansion(::onc::vpn::kType, &type);
  TranslateWithTableAndSet(type, kVPNTypeTable, shill::kProviderTypeProperty);

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateWiFi() {
  std::string security;
  onc_object_->GetStringWithoutPathExpansion(::onc::wifi::kSecurity, &security);
  TranslateWithTableAndSet(security, kWiFiSecurityTable,
                           shill::kSecurityProperty);

  std::string ssid;
  onc_object_->GetStringWithoutPathExpansion(::onc::wifi::kSSID, &ssid);
  shill_property_util::SetSSID(ssid, shill_dictionary_);

  // We currently only support managed and no adhoc networks.
  shill_dictionary_->SetStringWithoutPathExpansion(shill::kModeProperty,
                                                   shill::kModeManaged);
  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateEAP() {
  std::string outer;
  onc_object_->GetStringWithoutPathExpansion(::onc::eap::kOuter, &outer);
  TranslateWithTableAndSet(outer, kEAPOuterTable, shill::kEapMethodProperty);

  // Translate the inner protocol only for outer tunneling protocols.
  if (outer == ::onc::eap::kPEAP || outer == ::onc::eap::kEAP_TTLS) {
    // In ONC the Inner protocol defaults to "Automatic".
    std::string inner = ::onc::eap::kAutomatic;
    // ONC's Inner == "Automatic" translates to omitting the Phase2 property in
    // Shill.
    onc_object_->GetStringWithoutPathExpansion(::onc::eap::kInner, &inner);
    if (inner != ::onc::eap::kAutomatic) {
      const StringTranslationEntry* table =
          outer == ::onc::eap::kPEAP ? kEAP_PEAP_InnerTable :
                                       kEAP_TTLS_InnerTable;
      TranslateWithTableAndSet(inner, table, shill::kEapPhase2AuthProperty);
    }
  }

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::TranslateNetworkConfiguration() {
  std::string type;
  onc_object_->GetStringWithoutPathExpansion(::onc::network_config::kType,
                                             &type);

  // Set the type except for Ethernet which is set in TranslateEthernet.
  if (type != ::onc::network_type::kEthernet)
    TranslateWithTableAndSet(type, kNetworkTypeTable, shill::kTypeProperty);

  // Shill doesn't allow setting the name for non-VPN networks.
  if (type == ::onc::network_type::kVPN) {
    std::string name;
    onc_object_->GetStringWithoutPathExpansion(::onc::network_config::kName,
                                               &name);
    shill_dictionary_->SetStringWithoutPathExpansion(shill::kNameProperty,
                                                     name);
  }

  CopyFieldsAccordingToSignature();
}

void LocalTranslator::CopyFieldsAccordingToSignature() {
  for (base::DictionaryValue::Iterator it(*onc_object_); !it.IsAtEnd();
       it.Advance()) {
    AddValueAccordingToSignature(it.key(),
                                 make_scoped_ptr(it.value().DeepCopy()));
  }
}

void LocalTranslator::AddValueAccordingToSignature(
    const std::string& onc_name,
    scoped_ptr<base::Value> value) {
  if (!value || !field_translation_table_)
    return;
  std::string shill_property_name;
  if (!GetShillPropertyName(onc_name,
                            field_translation_table_,
                            &shill_property_name))
    return;

  shill_dictionary_->SetWithoutPathExpansion(shill_property_name,
                                             value.release());
}

void LocalTranslator::TranslateWithTableAndSet(
    const std::string& onc_value,
    const StringTranslationEntry table[],
    const std::string& shill_property_name) {
  std::string shill_value;
  if (TranslateStringToShill(table, onc_value, &shill_value)) {
    shill_dictionary_->SetStringWithoutPathExpansion(shill_property_name,
                                                     shill_value);
    return;
  }
  // As we previously validate ONC, this case should never occur. If it still
  // occurs, we should check here. Otherwise the failure will only show up much
  // later in Shill.
  LOG(ERROR) << "Value '" << onc_value
             << "' cannot be translated to Shill property "
             << shill_property_name;
}

// Iterates recursively over |onc_object| and its |signature|. At each object
// applies the local translation using LocalTranslator::TranslateFields. The
// results are written to |shill_dictionary|.
void TranslateONCHierarchy(const OncValueSignature& signature,
                           const base::DictionaryValue& onc_object,
                           base::DictionaryValue* shill_dictionary) {
  base::DictionaryValue* target_shill_dictionary = shill_dictionary;
  std::vector<std::string> path_to_shill_dictionary =
      GetPathToNestedShillDictionary(signature);
  for (std::vector<std::string>::const_iterator it =
           path_to_shill_dictionary.begin();
       it != path_to_shill_dictionary.end();
       ++it) {
    base::DictionaryValue* nested_shill_dict = NULL;
    target_shill_dictionary->GetDictionaryWithoutPathExpansion(
        *it, &nested_shill_dict);
    if (!nested_shill_dict)
      nested_shill_dict = new base::DictionaryValue;
    target_shill_dictionary->SetWithoutPathExpansion(*it, nested_shill_dict);
    target_shill_dictionary = nested_shill_dict;
  }
  // Translates fields of |onc_object| and writes them to
  // |target_shill_dictionary_| nested in |shill_dictionary|.
  LocalTranslator translator(signature, onc_object, target_shill_dictionary);
  translator.TranslateFields();

  // Recurse into nested objects.
  for (base::DictionaryValue::Iterator it(onc_object); !it.IsAtEnd();
       it.Advance()) {
    const base::DictionaryValue* inner_object = NULL;
    if (!it.value().GetAsDictionary(&inner_object))
      continue;

    const OncFieldSignature* field_signature =
        GetFieldSignature(signature, it.key());

    TranslateONCHierarchy(*field_signature->value_signature, *inner_object,
                          shill_dictionary);
  }
}

}  // namespace

scoped_ptr<base::DictionaryValue> TranslateONCObjectToShill(
    const OncValueSignature* onc_signature,
    const base::DictionaryValue& onc_object) {
  CHECK(onc_signature != NULL);
  scoped_ptr<base::DictionaryValue> shill_dictionary(new base::DictionaryValue);
  TranslateONCHierarchy(*onc_signature, onc_object, shill_dictionary.get());
  return shill_dictionary.Pass();
}

}  // namespace onc
}  // namespace chromeos
