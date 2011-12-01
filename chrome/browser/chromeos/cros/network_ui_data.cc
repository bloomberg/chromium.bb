// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_ui_data.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

// Top-level UI data dictionary keys.
const char NetworkUIData::kKeyONCSource[] = "onc_source";
const char NetworkUIData::kKeyProperties[] = "properties";

// Property names for per-property data stored under |kKeyProperties|.
const char NetworkUIData::kPropertyAutoConnect[] = "auto_connect";
const char NetworkUIData::kPropertyPreferred[] = "preferred";
const char NetworkUIData::kPropertyPassphrase[] = "passphrase";
const char NetworkUIData::kPropertySaveCredentials[] = "save_credentials";

const char NetworkUIData::kPropertyVPNCaCertNss[] = "VPN.ca_cert_nss";
const char NetworkUIData::kPropertyVPNPskPassphrase[] = "VPN.psk_passphrase";
const char NetworkUIData::kPropertyVPNClientCertId[] = "VPN.client_cert_id";
const char NetworkUIData::kPropertyVPNUsername[] = "VPN.username";
const char NetworkUIData::kPropertyVPNUserPassphrase[] = "VPN.user_passphrase";
const char NetworkUIData::kPropertyVPNGroupName[] = "VPN.group_name";

const char NetworkUIData::kPropertyEAPMethod[] = "EAP.method";
const char NetworkUIData::kPropertyEAPPhase2Auth[] = "EAP.phase_2_auth";
const char NetworkUIData::kPropertyEAPServerCaCertNssNickname[] =
    "EAP.server_ca_cert_nss_nickname";
const char NetworkUIData::kPropertyEAPClientCertPkcs11Id[] =
    "EAP.client_cert_pkcs11_id";
const char NetworkUIData::kPropertyEAPUseSystemCAs[] = "EAP.use_system_cas";
const char NetworkUIData::kPropertyEAPIdentity[] = "EAP.identity";
const char NetworkUIData::kPropertyEAPAnonymousIdentity[] =
    "EAP.anonymous_identity";
const char NetworkUIData::kPropertyEAPPassphrase[] = "EAP.passphrase";

const EnumMapper<NetworkUIData::ONCSource>::Pair
    NetworkUIData::kONCSourceTable[] =  {
  { "user_import", NetworkUIData::ONC_SOURCE_USER_IMPORT },
  { "device_policy", NetworkUIData::ONC_SOURCE_DEVICE_POLICY },
  { "user_policy", NetworkUIData::ONC_SOURCE_USER_POLICY },
};

// Property names for the per-property dictionary.
const char NetworkPropertyUIData::kKeyController[] = "controller";
const char NetworkPropertyUIData::kKeyDefaultValue[] = "default_value";

const EnumMapper<NetworkPropertyUIData::Controller>::Pair
    NetworkPropertyUIData::kControllerTable[] =  {
  { "user", NetworkPropertyUIData::CONTROLLER_USER },
  { "policy", NetworkPropertyUIData::CONTROLLER_POLICY },
};

NetworkUIData::NetworkUIData()
  : onc_source_(ONC_SOURCE_NONE) {
}

NetworkUIData::~NetworkUIData() {
}

void NetworkUIData::SetProperty(const char* property_key,
                                const NetworkPropertyUIData& ui_data) {
  properties_.Set(property_key, ui_data.BuildDictionary());
}

void NetworkUIData::FillDictionary(base::DictionaryValue* dict) const {
  std::string source_string(GetONCSourceMapper().GetKey(onc_source_));
  if (!source_string.empty())
    dict->SetString(kKeyONCSource, source_string);
  dict->Set(kKeyProperties, properties_.DeepCopy());
}

// static
NetworkUIData::ONCSource NetworkUIData::GetONCSource(const Network* network) {
  std::string source;
  if (network->ui_data()->GetString(kKeyONCSource, &source))
    return GetONCSourceMapper().Get(source);
  return ONC_SOURCE_NONE;
}

// static
bool NetworkUIData::IsManaged(const Network* network) {
  ONCSource source = GetONCSource(network);
  return source == ONC_SOURCE_DEVICE_POLICY || source == ONC_SOURCE_USER_POLICY;
}

// static
EnumMapper<NetworkUIData::ONCSource>& NetworkUIData::GetONCSourceMapper() {
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ONCSource>, mapper,
                         (kONCSourceTable, arraysize(kONCSourceTable),
                          ONC_SOURCE_NONE));
  return mapper;
}

NetworkPropertyUIData::NetworkPropertyUIData()
    : controller_(CONTROLLER_USER) {
}

NetworkPropertyUIData::~NetworkPropertyUIData() {
}

NetworkPropertyUIData::NetworkPropertyUIData(Controller controller,
                                             base::Value* default_value)
    : controller_(controller),
      default_value_(default_value) {
}

NetworkPropertyUIData::NetworkPropertyUIData(const Network* network,
                                             const char* property_key) {
  UpdateFromNetwork(network, property_key);
}

void NetworkPropertyUIData::UpdateFromNetwork(const Network* network,
                                              const char* property_key) {
  // If there is no per-property information available, the property inherits
  // the controlled state of the network.
  controller_ =
      NetworkUIData::IsManaged(network) ? CONTROLLER_POLICY : CONTROLLER_USER;
  default_value_.reset();

  if (!property_key)
    return;

  const base::DictionaryValue* ui_data = network->ui_data();
  if (!ui_data)
    return;

  base::DictionaryValue* property_map = NULL;
  if (!ui_data->GetDictionary(NetworkUIData::kKeyProperties, &property_map))
    return;

  base::DictionaryValue* property = NULL;
  if (!property_map->GetDictionary(property_key, &property))
    return;

  std::string controller;
  if (property->GetString(kKeyController, &controller))
    controller_ = GetControllerMapper().Get(controller);

  base::Value* default_value = NULL;
  if (property->Get(kKeyDefaultValue, &default_value) && default_value)
    default_value_.reset(default_value->DeepCopy());
}

base::DictionaryValue* NetworkPropertyUIData::BuildDictionary() const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  std::string controller_string(GetControllerMapper().GetKey(controller_));
  if (!controller_string.empty())
    dict->SetString(kKeyController, controller_string);
  if (default_value_.get())
    dict->Set(kKeyDefaultValue, default_value_->DeepCopy());
  return dict;
}

// static
EnumMapper<NetworkPropertyUIData::Controller>&
    NetworkPropertyUIData::GetControllerMapper() {
  CR_DEFINE_STATIC_LOCAL(EnumMapper<Controller>, mapper,
                         (kControllerTable, arraysize(kControllerTable),
                          CONTROLLER_USER));
  return mapper;
}

}  // namespace chromeos
