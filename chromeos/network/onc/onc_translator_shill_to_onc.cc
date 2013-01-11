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
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace onc {

namespace {

// Converts |str| to a base::Value of the given |type|. If the conversion fails,
// returns NULL.
scoped_ptr<base::Value> ConvertStringToValue(const std::string& str,
                                             base::Value::Type type) {
  base::Value* value;
  if (type == base::Value::TYPE_STRING)
    value = base::Value::CreateStringValue(str);
  else
    value = base::JSONReader::Read(str);

  if (value == NULL || value->GetType() != type) {
    delete value;
    value = NULL;
  }
  return make_scoped_ptr(value);
}

// This class implements the translation of properties from the given
// |shill_dictionary| to a new ONC object of signature |onc_signature|. Using
// recursive calls to TranslateShillServiceToONCPart, nested objects are
// translated.
class ShillToONCTranslator {
 public:
  ShillToONCTranslator(const base::DictionaryValue& shill_dictionary,
                       const OncValueSignature& onc_signature)
      : shill_dictionary_(&shill_dictionary),
        onc_signature_(&onc_signature) {
  }

  // Translates the associated Shill dictionary and creates an ONC object of the
  // given signature.
  scoped_ptr<base::DictionaryValue> CreateTranslatedONCObject();

 private:
  void TranslateOpenVPN();
  void TranslateVPN();
  void TranslateNetworkConfiguration();

  // Creates an ONC object from |shill_dictionary| according to the signature
  // associated to |onc_field_name| and adds it to |onc_object_| at
  // |onc_field_name|.
  void TranslateAndAddNestedObject(const std::string& onc_field_name);

  // Copies all entries from |shill_dictionary_| to |onc_object_| for which a
  // translation (shill_property_name) is defined by |onc_signature_|.
  void CopyPropertiesAccordingToSignature();

  // If existent, translates the entry at |shill_property_name| in
  // |shill_dictionary_| using |table|. It is an error if no matching table
  // entry is found. Writes the result as entry at |onc_field_name| in
  // |onc_object_|.
  void TranslateWithTableAndSet(const std::string& shill_property_name,
                                const StringTranslationEntry table[],
                                const std::string& onc_field_name);

  const base::DictionaryValue* shill_dictionary_;
  const OncValueSignature* onc_signature_;
  scoped_ptr<base::DictionaryValue> onc_object_;

  DISALLOW_COPY_AND_ASSIGN(ShillToONCTranslator);
};

scoped_ptr<base::DictionaryValue>
ShillToONCTranslator::CreateTranslatedONCObject() {
  onc_object_.reset(new base::DictionaryValue);
  if (onc_signature_ == &kNetworkConfigurationSignature)
    TranslateNetworkConfiguration();
  else if (onc_signature_ == &kVPNSignature)
    TranslateVPN();
  else if (onc_signature_ == &kOpenVPNSignature)
    TranslateOpenVPN();
  else
    CopyPropertiesAccordingToSignature();
  return onc_object_.Pass();
}

void ShillToONCTranslator::TranslateOpenVPN() {
  // Shill supports only one RemoteCertKU but ONC requires a list. If existing,
  // wraps the value into a list.
  std::string certKU;
  if (shill_dictionary_->GetStringWithoutPathExpansion(
          flimflam::kOpenVPNRemoteCertKUProperty, &certKU)) {
    scoped_ptr<base::ListValue> certKUs(new base::ListValue);
    certKUs->AppendString(certKU);
    onc_object_->SetWithoutPathExpansion(vpn::kRemoteCertKU, certKUs.release());
  }

  for (const OncFieldSignature* field_signature = onc_signature_->fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    const base::Value* shill_value;
    if (field_signature->shill_property_name == NULL ||
        !shill_dictionary_->GetWithoutPathExpansion(
            field_signature->shill_property_name, &shill_value)) {
      continue;
    }

    scoped_ptr<base::Value> translated;
    std::string shill_str;
    const std::string& onc_field_name = field_signature->onc_field_name;
    if (onc_field_name == vpn::kSaveCredentials ||
        onc_field_name == vpn::kRemoteCertKU) {
      translated.reset(shill_value->DeepCopy());
    } else if (shill_value->GetAsString(&shill_str)) {
      // Shill wants all Provider/VPN fields to be strings. Translates these
      // strings back to the correct ONC type.
      translated = ConvertStringToValue(
          shill_str,
          field_signature->value_signature->onc_type);

      if (translated.get() == NULL) {
        LOG(ERROR) << "Shill property '" << field_signature->shill_property_name
                   << "' with value '" << shill_value
                   << "' couldn't be converted to base::Value::Type "
                   << field_signature->value_signature->onc_type;
      }
    } else {
      LOG(ERROR) << "Shill property '" << field_signature->shill_property_name
                 << "' has value '" << shill_value
                 << "', but expected a string";
    }
    onc_object_->SetWithoutPathExpansion(onc_field_name, translated.release());
  }
}

void ShillToONCTranslator::TranslateVPN() {
  TranslateWithTableAndSet(flimflam::kProviderTypeProperty, kVPNTypeTable,
                           kType);
  CopyPropertiesAccordingToSignature();

  std::string vpn_type;
  if (onc_object_->GetStringWithoutPathExpansion(kType, &vpn_type)) {
    if (vpn_type == vpn::kTypeL2TP_IPsec) {
      TranslateAndAddNestedObject(vpn::kIPsec);
      TranslateAndAddNestedObject(vpn::kL2TP);
    } else {
      TranslateAndAddNestedObject(vpn_type);
    }
  }
}

void ShillToONCTranslator::TranslateAndAddNestedObject(
    const std::string& onc_field_name) {
  const OncFieldSignature* field_signature =
      GetFieldSignature(*onc_signature_, onc_field_name);
  ShillToONCTranslator nested_translator(*shill_dictionary_,
                                         *field_signature->value_signature);
  scoped_ptr<base::DictionaryValue> nested_object =
      nested_translator.CreateTranslatedONCObject();
  onc_object_->SetWithoutPathExpansion(onc_field_name, nested_object.release());
}

void ShillToONCTranslator::TranslateNetworkConfiguration() {
  TranslateWithTableAndSet(flimflam::kTypeProperty, kNetworkTypeTable, kType);
  CopyPropertiesAccordingToSignature();

  std::string network_type;
  if (onc_object_->GetStringWithoutPathExpansion(kType, &network_type))
    TranslateAndAddNestedObject(network_type);

  if (network_type == kVPN) {
    std::string name;
    shill_dictionary_->GetStringWithoutPathExpansion(flimflam::kNameProperty,
                                                     &name);
    onc_object_->SetStringWithoutPathExpansion(kName, name);
  }
}

void ShillToONCTranslator::CopyPropertiesAccordingToSignature() {
  for (const OncFieldSignature* field_signature = onc_signature_->fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    const base::Value* shill_value;
    if (field_signature->shill_property_name == NULL ||
        !shill_dictionary_->GetWithoutPathExpansion(
            field_signature->shill_property_name, &shill_value)) {
      continue;
    }
    onc_object_->SetWithoutPathExpansion(
        field_signature->onc_field_name, shill_value->DeepCopy());
  }
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

  for (int i = 0; table[i].onc_value != NULL; ++i) {
    if (shill_value != table[i].shill_value)
      continue;
    onc_object_->SetStringWithoutPathExpansion(onc_field_name,
                                               table[i].onc_value);
    return;
  }
  LOG(ERROR) << "Shill property '" << shill_property_name << "' with value '"
             << shill_value << "' couldn't be translated to ONC";
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
