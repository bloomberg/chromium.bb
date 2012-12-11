// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/onc_network_parser.h"

#include <keyhi.h>
#include <pk11pub.h>

#include "base/base64.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"  // for debug output only.
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/certificate_pattern.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/native_network_constants.h"
#include "chrome/browser/chromeos/cros/native_network_parser.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_validator.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/encryptor.h"
#include "crypto/hmac.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/symmetric_key.h"
#include "grit/generated_resources.h"
#include "net/base/crypto_module.h"
#include "net/base/net_errors.h"
#include "net/base/nss_cert_database.h"
#include "net/base/pem_tokenizer.h"
#include "net/base/x509_certificate.h"
#include "net/proxy/proxy_bypass_rules.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

// Local constants.
namespace {

// The PEM block header used for DER certificates
const char kCertificateHeader[] = "CERTIFICATE";
// This is an older PEM marker for DER certificates.
const char kX509CertificateHeader[] = "X509 CERTIFICATE";

const base::Value::Type TYPE_BOOLEAN = base::Value::TYPE_BOOLEAN;
const base::Value::Type TYPE_DICTIONARY = base::Value::TYPE_DICTIONARY;
const base::Value::Type TYPE_INTEGER = base::Value::TYPE_INTEGER;
const base::Value::Type TYPE_LIST = base::Value::TYPE_LIST;
const base::Value::Type TYPE_STRING = base::Value::TYPE_STRING;

// Only used currently to keep NetworkParser superclass happy.
EnumMapper<PropertyIndex>::Pair network_configuration_table[] = {
  { "GUID", PROPERTY_INDEX_GUID }
};

OncValueSignature network_configuration_signature[] = {
  { onc::kGUID, PROPERTY_INDEX_GUID, TYPE_STRING },
  { onc::kProxySettings, PROPERTY_INDEX_ONC_PROXY_SETTINGS, TYPE_DICTIONARY },
  { onc::kName, PROPERTY_INDEX_NAME, TYPE_STRING },
  { onc::kRemove, PROPERTY_INDEX_ONC_REMOVE, TYPE_BOOLEAN },
  { onc::kType, PROPERTY_INDEX_TYPE, TYPE_STRING },
  { onc::kEthernet, PROPERTY_INDEX_ONC_ETHERNET, TYPE_DICTIONARY },
  { onc::kWiFi, PROPERTY_INDEX_ONC_WIFI, TYPE_DICTIONARY },
  { onc::kVPN, PROPERTY_INDEX_ONC_VPN, TYPE_DICTIONARY },
  { NULL }
};

OncValueSignature ethernet_signature[] = {
  { onc::ethernet::kAuthentication, PROPERTY_INDEX_AUTHENTICATION,
    TYPE_STRING },
  { onc::ethernet::kEAP, PROPERTY_INDEX_EAP, TYPE_DICTIONARY },
  { NULL }
};

OncValueSignature wifi_signature[] = {
  { onc::wifi::kAutoConnect, PROPERTY_INDEX_AUTO_CONNECT, TYPE_BOOLEAN },
  { onc::wifi::kEAP, PROPERTY_INDEX_EAP, TYPE_DICTIONARY },
  { onc::wifi::kHiddenSSID, PROPERTY_INDEX_WIFI_HIDDEN_SSID, TYPE_BOOLEAN },
  { onc::wifi::kPassphrase, PROPERTY_INDEX_PASSPHRASE, TYPE_STRING },
  { onc::wifi::kSecurity, PROPERTY_INDEX_SECURITY, TYPE_STRING },
  { onc::wifi::kSSID, PROPERTY_INDEX_SSID, TYPE_STRING },
  { NULL }
};

OncValueSignature issuer_subject_pattern_signature[] = {
  { onc::certificate::kCommonName,
    PROPERTY_INDEX_ISSUER_SUBJECT_PATTERN_COMMON_NAME, TYPE_STRING },
  { onc::certificate::kLocality,
    PROPERTY_INDEX_ISSUER_SUBJECT_PATTERN_LOCALITY, TYPE_STRING },
  { onc::certificate::kOrganization,
    PROPERTY_INDEX_ISSUER_SUBJECT_PATTERN_ORGANIZATION, TYPE_STRING },
  { onc::certificate::kOrganizationalUnit,
    PROPERTY_INDEX_ISSUER_SUBJECT_PATTERN_ORGANIZATIONAL_UNIT,
    TYPE_STRING },
};

OncValueSignature certificate_pattern_signature[] = {
  { onc::certificate::kIssuerCARef,
    PROPERTY_INDEX_ONC_CERTIFICATE_PATTERN_ISSUER_CA_REF, TYPE_LIST },
  { onc::certificate::kIssuer,
    PROPERTY_INDEX_ONC_CERTIFICATE_PATTERN_ISSUER, TYPE_DICTIONARY },
  { onc::certificate::kSubject,
    PROPERTY_INDEX_ONC_CERTIFICATE_PATTERN_SUBJECT, TYPE_DICTIONARY },
  { onc::certificate::kEnrollmentURI,
    PROPERTY_INDEX_ONC_CERTIFICATE_PATTERN_ENROLLMENT_URI, TYPE_LIST },
};

OncValueSignature eap_signature[] = {
  { onc::eap::kAnonymousIdentity, PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY,
    TYPE_STRING },
  { onc::eap::kClientCertPattern, PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN,
    TYPE_DICTIONARY },
  { onc::eap::kClientCertRef, PROPERTY_INDEX_ONC_CLIENT_CERT_REF, TYPE_STRING },
  { onc::eap::kClientCertType, PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE,
    TYPE_STRING },
  { onc::eap::kIdentity, PROPERTY_INDEX_EAP_IDENTITY, TYPE_STRING },
  { onc::eap::kInner, PROPERTY_INDEX_EAP_PHASE_2_AUTH, TYPE_STRING },
  { onc::eap::kOuter, PROPERTY_INDEX_EAP_METHOD, TYPE_STRING },
  { onc::eap::kPassword, PROPERTY_INDEX_EAP_PASSWORD, TYPE_STRING },
  { onc::eap::kServerCARef, PROPERTY_INDEX_EAP_CA_CERT_NSS, TYPE_STRING },
  { onc::eap::kUseSystemCAs, PROPERTY_INDEX_EAP_USE_SYSTEM_CAS, TYPE_BOOLEAN },
  { onc::eap::kSaveCredentials, PROPERTY_INDEX_SAVE_CREDENTIALS, TYPE_BOOLEAN },
  { NULL }
};

OncValueSignature vpn_signature[] = {
  { onc::vpn::kHost, PROPERTY_INDEX_PROVIDER_HOST, TYPE_STRING },
  { onc::vpn::kIPsec, PROPERTY_INDEX_ONC_IPSEC, TYPE_DICTIONARY },
  { onc::vpn::kL2TP, PROPERTY_INDEX_ONC_L2TP, TYPE_DICTIONARY },
  { onc::vpn::kOpenVPN, PROPERTY_INDEX_ONC_OPENVPN, TYPE_DICTIONARY },
  { onc::vpn::kType, PROPERTY_INDEX_PROVIDER_TYPE, TYPE_STRING },
  { NULL }
};

OncValueSignature ipsec_signature[] = {
  { onc::vpn::kAuthenticationType, PROPERTY_INDEX_IPSEC_AUTHENTICATIONTYPE,
    TYPE_STRING },
  { onc::vpn::kGroup, PROPERTY_INDEX_L2TPIPSEC_GROUP_NAME, TYPE_STRING },
  { onc::vpn::kIKEVersion, PROPERTY_INDEX_IPSEC_IKEVERSION, TYPE_INTEGER },
  { onc::vpn::kClientCertPattern, PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN,
    TYPE_DICTIONARY },
  { onc::vpn::kClientCertRef, PROPERTY_INDEX_ONC_CLIENT_CERT_REF, TYPE_STRING },
  { onc::vpn::kClientCertType, PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE,
    TYPE_STRING },
  // Note: EAP and XAUTH not yet supported.
  { onc::vpn::kPSK, PROPERTY_INDEX_L2TPIPSEC_PSK, TYPE_STRING },
  { onc::vpn::kSaveCredentials, PROPERTY_INDEX_SAVE_CREDENTIALS, TYPE_BOOLEAN },
  { onc::vpn::kServerCARef, PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS, TYPE_STRING },
  { NULL }
};

OncValueSignature l2tp_signature[] = {
  { onc::vpn::kPassword, PROPERTY_INDEX_L2TPIPSEC_PASSWORD, TYPE_STRING },
  { onc::vpn::kSaveCredentials, PROPERTY_INDEX_SAVE_CREDENTIALS, TYPE_BOOLEAN },
  { onc::vpn::kUsername, PROPERTY_INDEX_L2TPIPSEC_USER, TYPE_STRING },
  { NULL }
};

OncValueSignature openvpn_signature[] = {
  { onc::vpn::kAuth, PROPERTY_INDEX_OPEN_VPN_AUTH, TYPE_STRING },
  { onc::vpn::kAuthRetry, PROPERTY_INDEX_OPEN_VPN_AUTHRETRY, TYPE_STRING },
  { onc::vpn::kAuthNoCache, PROPERTY_INDEX_OPEN_VPN_AUTHNOCACHE, TYPE_BOOLEAN },
  { onc::vpn::kCipher, PROPERTY_INDEX_OPEN_VPN_CIPHER, TYPE_STRING },
  { onc::vpn::kClientCertPattern, PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN,
    TYPE_DICTIONARY },
  { onc::vpn::kClientCertRef, PROPERTY_INDEX_ONC_CLIENT_CERT_REF, TYPE_STRING },
  { onc::vpn::kClientCertType, PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE,
    TYPE_STRING },
  { onc::vpn::kCompLZO, PROPERTY_INDEX_OPEN_VPN_COMPLZO, TYPE_STRING },
  { onc::vpn::kCompNoAdapt, PROPERTY_INDEX_OPEN_VPN_COMPNOADAPT, TYPE_BOOLEAN },
  { onc::vpn::kKeyDirection, PROPERTY_INDEX_OPEN_VPN_KEYDIRECTION,
    TYPE_STRING },
  { onc::vpn::kNsCertType, PROPERTY_INDEX_OPEN_VPN_NSCERTTYPE, TYPE_STRING },
  { onc::vpn::kPassword, PROPERTY_INDEX_OPEN_VPN_PASSWORD, TYPE_STRING },
  { onc::vpn::kPort, PROPERTY_INDEX_OPEN_VPN_PORT, TYPE_INTEGER },
  { onc::vpn::kProto, PROPERTY_INDEX_OPEN_VPN_PROTO, TYPE_STRING },
  { onc::vpn::kPushPeerInfo, PROPERTY_INDEX_OPEN_VPN_PUSHPEERINFO,
    TYPE_BOOLEAN },
  { onc::vpn::kRemoteCertEKU, PROPERTY_INDEX_OPEN_VPN_REMOTECERTEKU,
    TYPE_STRING },
  { onc::vpn::kRemoteCertKU, PROPERTY_INDEX_OPEN_VPN_REMOTECERTKU, TYPE_LIST },
  { onc::vpn::kRemoteCertTLS, PROPERTY_INDEX_OPEN_VPN_REMOTECERTTLS,
    TYPE_STRING },
  { onc::vpn::kRenegSec, PROPERTY_INDEX_OPEN_VPN_RENEGSEC, TYPE_INTEGER },
  { onc::vpn::kSaveCredentials, PROPERTY_INDEX_SAVE_CREDENTIALS, TYPE_BOOLEAN },
  { onc::vpn::kServerCARef, PROPERTY_INDEX_OPEN_VPN_CACERT, TYPE_STRING },
  { onc::vpn::kServerCertRef, PROPERTY_INDEX_OPEN_VPN_CERT, TYPE_STRING },
  { onc::vpn::kServerPollTimeout, PROPERTY_INDEX_OPEN_VPN_SERVERPOLLTIMEOUT,
    TYPE_INTEGER },
  { onc::vpn::kShaper, PROPERTY_INDEX_OPEN_VPN_SHAPER, TYPE_INTEGER },
  { onc::vpn::kStaticChallenge, PROPERTY_INDEX_OPEN_VPN_STATICCHALLENGE,
    TYPE_STRING },
  { onc::vpn::kTLSAuthContents, PROPERTY_INDEX_OPEN_VPN_TLSAUTHCONTENTS,
    TYPE_STRING },
  { onc::vpn::kTLSRemote, PROPERTY_INDEX_OPEN_VPN_TLSREMOTE, TYPE_STRING },
  { onc::vpn::kUsername, PROPERTY_INDEX_OPEN_VPN_USER, TYPE_STRING },
  { NULL }
};

OncValueSignature proxy_settings_signature[] = {
  { onc::proxy::kType, PROPERTY_INDEX_ONC_PROXY_TYPE, TYPE_STRING },
  { onc::proxy::kPAC, PROPERTY_INDEX_ONC_PROXY_PAC, TYPE_STRING },
  { onc::proxy::kManual, PROPERTY_INDEX_ONC_PROXY_MANUAL, TYPE_DICTIONARY },
  { onc::proxy::kExcludeDomains, PROPERTY_INDEX_ONC_PROXY_EXCLUDE_DOMAINS,
    TYPE_LIST },
  { NULL },
};

OncValueSignature proxy_manual_signature[] = {
  { onc::proxy::kHttp, PROPERTY_INDEX_ONC_PROXY_HTTP, TYPE_DICTIONARY },
  { onc::proxy::kHttps, PROPERTY_INDEX_ONC_PROXY_HTTPS, TYPE_DICTIONARY },
  { onc::proxy::kFtp, PROPERTY_INDEX_ONC_PROXY_FTP, TYPE_DICTIONARY },
  { onc::proxy::kSocks, PROPERTY_INDEX_ONC_PROXY_SOCKS, TYPE_DICTIONARY },
  { NULL },
};

// Serve the singleton mapper instance.
const EnumMapper<PropertyIndex>* get_onc_mapper() {
  CR_DEFINE_STATIC_LOCAL(const EnumMapper<PropertyIndex>, mapper,
      (network_configuration_table,
       arraysize(network_configuration_table),
       PROPERTY_INDEX_UNKNOWN));
  return &mapper;
}

ConnectionType ParseNetworkType(const std::string& type) {
  static EnumMapper<ConnectionType>::Pair table[] = {
    { "Ethernet", TYPE_ETHERNET },
    { "WiFi", TYPE_WIFI },
    { "VPN", TYPE_VPN },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ConnectionType>, parser,
      (table, arraysize(table), TYPE_UNKNOWN));
  return parser.Get(type);
}

std::string GetStringValue(const base::Value& value) {
  std::string string_value;
  value.GetAsString(&string_value);
  return string_value;
}

const bool GetBooleanValue(const base::Value& value) {
  bool bool_value = false;
  value.GetAsBoolean(&bool_value);
  return bool_value;
}

std::string ConvertValueToString(const base::Value& value) {
  std::string value_json;
  base::JSONWriter::Write(&value, &value_json);
  return value_json;
}

bool GetAsListOfStrings(const base::Value& value,
                        std::vector<std::string>* result) {
  const base::ListValue* list = NULL;
  if (!value.GetAsList(&list))
    return false;
  result->clear();
  result->reserve(list->GetSize());
  for (size_t i = 0; i < list->GetSize(); i++) {
    std::string item;
    if (!list->GetString(i, &item))
      return false;
    result->push_back(item);
  }
  return true;
}

}  // namespace

// -------------------- OncNetworkParser --------------------

OncNetworkParser::OncNetworkParser(const base::ListValue& network_configs,
                                   onc::ONCSource onc_source)
    : NetworkParser(get_onc_mapper()),
      onc_source_(onc_source),
      network_configs_(network_configs.DeepCopy()) {
}

OncNetworkParser::OncNetworkParser()
    : NetworkParser(get_onc_mapper()),
      network_configs_(NULL) {
}

OncNetworkParser::~OncNetworkParser() {
}

// static
const EnumMapper<PropertyIndex>* OncNetworkParser::property_mapper() {
  return get_onc_mapper();
}

int OncNetworkParser::GetNetworkConfigsSize() const {
  return network_configs_ ? network_configs_->GetSize() : 0;
}

const base::DictionaryValue* OncNetworkParser::GetNetworkConfig(int n) {
  CHECK(network_configs_);
  CHECK(static_cast<size_t>(n) < network_configs_->GetSize());
  CHECK_GE(n, 0);
  base::DictionaryValue* info = NULL;
  if (!network_configs_->GetDictionary(n, &info)) {
    parse_error_ = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_NETWORK_PROP_DICT_MALFORMED);
    return NULL;
  }

  return info;
}

Network* OncNetworkParser::ParseNetwork(int n, bool* marked_for_removal) {
  const base::DictionaryValue* info = GetNetworkConfig(n);
  if (!info)
    return NULL;

  if (VLOG_IS_ON(2)) {
    std::string network_json;
    base::JSONWriter::WriteWithOptions(static_cast<const base::Value*>(info),
                                       base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                       &network_json);
    VLOG(2) << "Parsing network at index " << n << ": " << network_json;
  }

  if (marked_for_removal) {
    if (!info->GetBoolean(onc::kRemove, marked_for_removal))
      *marked_for_removal = false;
  }

  return CreateNetworkFromInfo(std::string(), *info);
}

Network* OncNetworkParser::CreateNetworkFromInfo(
    const std::string& service_path,
    const DictionaryValue& info) {
  ConnectionType type = ParseTypeFromDictionary(info);
  if (type == TYPE_UNKNOWN) {  // Return NULL if cannot parse network type.
    parse_error_ = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_NETWORK_TYPE_MISSING);
    return NULL;
  }
  scoped_ptr<Network> network(CreateNewNetwork(type, service_path));

  // Initialize ONC source.
  NetworkUIData ui_data = network->ui_data();
  ui_data.set_onc_source(onc_source_);
  network->set_ui_data(ui_data);

  // Parse all properties recursively.
  if (!ParseNestedObject(network.get(),
                         onc::kNetworkConfiguration,
                         static_cast<const base::Value&>(info),
                         network_configuration_signature,
                         ParseNetworkConfigurationValue)) {
    LOG(WARNING) << "Network " << network->name() << " failed to parse.";
    if (parse_error_.empty())
      parse_error_ = l10n_util::GetStringUTF8(
          IDS_NETWORK_CONFIG_ERROR_NETWORK_PROP_DICT_MALFORMED);
    return NULL;
  }

  // Update the UI data property in shill.
  std::string ui_data_json;
  base::DictionaryValue ui_data_dict;
  network->ui_data().FillDictionary(&ui_data_dict);
  base::JSONWriter::Write(&ui_data_dict, &ui_data_json);
  base::StringValue ui_data_string_value(ui_data_json);
  network->UpdatePropertyMap(PROPERTY_INDEX_UI_DATA, &ui_data_string_value);

  if (VLOG_IS_ON(2)) {
    VLOG(2) << "Created Network '" << network->name()
            << "' from info. Path:" << service_path
            << " Type:" << ConnectionTypeToString(type);
  }

  return network.release();
}

bool OncNetworkParser::ParseNestedObject(Network* network,
                                         const std::string& onc_type,
                                         const base::Value& value,
                                         OncValueSignature* signature,
                                         ParserPointer parser) {
  bool any_errors = false;
  if (!value.IsType(base::Value::TYPE_DICTIONARY)) {
    VLOG(1) << network->name() << ": expected object of type " << onc_type;
    parse_error_ = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_NETWORK_PROP_DICT_MALFORMED);
    return false;
  }
  VLOG(2) << "Parsing nested object of type " << onc_type;
  const DictionaryValue* dict = NULL;
  value.GetAsDictionary(&dict);
  for (DictionaryValue::key_iterator iter = dict->begin_keys();
       iter != dict->end_keys(); ++iter) {
    const std::string& key = *iter;

    // Recommended keys are only of interest to the UI code and the UI reads it
    // directly from the ONC blob.
    if (key == onc::kRecommended)
      continue;

    const base::Value* inner_value = NULL;
    dict->GetWithoutPathExpansion(key, &inner_value);
    CHECK(inner_value != NULL);
    int field_index;
    for (field_index = 0; signature[field_index].field != NULL; ++field_index) {
      if (key == signature[field_index].field)
        break;
    }
    if (signature[field_index].field == NULL) {
      LOG(WARNING) << network->name() << ": unexpected field: "
                   << key << ", in type: " << onc_type;
      continue;
    }
    if (!inner_value->IsType(signature[field_index].type)) {
      LOG(ERROR) << network->name() << ": field with wrong type: " << key
                 << ", actual type: " << inner_value->GetType()
                 << ", expected type: " << signature[field_index].type;
      any_errors = true;
      continue;
    }
    PropertyIndex index = signature[field_index].index;
    // We need to UpdatePropertyMap now since parser might want to
    // change the mapped value.
    network->UpdatePropertyMap(index, inner_value);
    if (!parser(this, index, *inner_value, network)) {
      LOG(ERROR) << network->name() << ": field in " << onc_type
                 << " not parsed: " << key;
      any_errors = true;
      continue;
    }
    if (VLOG_IS_ON(2)) {
      std::string value_json;
      base::JSONWriter::WriteWithOptions(inner_value,
                                         base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                         &value_json);
      VLOG(2) << network->name() << ": Successfully parsed [" << key
              << "(" << index << ")] = " << value_json;
    }
  }
  if (any_errors) {
    parse_error_ = l10n_util::GetStringUTF8(
        IDS_NETWORK_CONFIG_ERROR_NETWORK_PROP_DICT_MALFORMED);
  }
  return !any_errors;
}

// static
std::string OncNetworkParser::GetUserExpandedValue(
    const base::Value& value,
    onc::ONCSource source) {
  std::string string_value;
  if (!value.GetAsString(&string_value))
    return string_value;

  // If running unit test, just return the original value.
  if (!content::BrowserThread::IsMessageLoopValid(content::BrowserThread::UI))
    return string_value;

  if (source != onc::ONC_SOURCE_USER_POLICY &&
      source != onc::ONC_SOURCE_USER_IMPORT) {
    return string_value;
  }

  if (!UserManager::Get()->IsUserLoggedIn())
    return string_value;

  const User* logged_in_user = UserManager::Get()->GetLoggedInUser();
  ReplaceSubstringsAfterOffset(&string_value, 0,
                               onc::substitutes::kLoginIDField,
                               logged_in_user->GetAccountName(false));
  ReplaceSubstringsAfterOffset(&string_value, 0,
                               onc::substitutes::kEmailField,
                               logged_in_user->email());
  return string_value;
}

Network* OncNetworkParser::CreateNewNetwork(
    ConnectionType type, const std::string& service_path) {
  Network* network = NetworkParser::CreateNewNetwork(type, service_path);
  if (network) {
    if (type == TYPE_ETHERNET)
      network->SetNetworkParser(new OncEthernetNetworkParser());
    else if (type == TYPE_WIFI)
      network->SetNetworkParser(new OncWifiNetworkParser());
    else if (type == TYPE_VPN)
      network->SetNetworkParser(new OncVirtualNetworkParser());
  }
  return network;
}

ConnectionType OncNetworkParser::ParseType(const std::string& type) {
  return ParseNetworkType(type);
}

ConnectionType OncNetworkParser::ParseTypeFromDictionary(
    const DictionaryValue& info) {
  return ParseType(GetTypeFromDictionary(info));
}

std::string OncNetworkParser::GetTypeFromDictionary(
    const base::DictionaryValue& info) {
  std::string type_string;
  info.GetString("Type", &type_string);
  return type_string;
}

std::string OncNetworkParser::GetGuidFromDictionary(
    const base::DictionaryValue& info) {
  std::string guid_string;
  info.GetString("GUID", &guid_string);
  return guid_string;
}

// static
bool OncNetworkParser::ParseNetworkConfigurationValue(
    OncNetworkParser* parser,
    PropertyIndex index,
    const base::Value& value,
    Network* network) {
  switch (index) {
    case PROPERTY_INDEX_ONC_ETHERNET:
      return parser->ParseNestedObject(network, "Ethernet", value,
          ethernet_signature, OncEthernetNetworkParser::ParseEthernetValue);
    case PROPERTY_INDEX_ONC_WIFI:
      return parser->ParseNestedObject(network,
                                       onc::kWiFi,
                                       value,
                                       wifi_signature,
                                       OncWifiNetworkParser::ParseWifiValue);
    case PROPERTY_INDEX_ONC_VPN: {
      if (!CheckNetworkType(network, TYPE_VPN, onc::kVPN))
        return false;
      VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
      // Got the "VPN" field.  Immediately store the VPN.Type field
      // value so that we can properly validate fields in the VPN
      // object based on the type.
      const DictionaryValue* dict = NULL;
      CHECK(value.GetAsDictionary(&dict));
      std::string provider_type_string;
      if (!dict->GetString("Type", &provider_type_string)) {
        VLOG(1) << network->name() << ": VPN.Type is missing";
        return false;
      }
      ProviderType provider_type =
        OncVirtualNetworkParser::ParseProviderType(provider_type_string);
      virtual_network->set_provider_type(provider_type);
      return parser->ParseNestedObject(network,
                                       onc::kVPN,
                                       value,
                                       vpn_signature,
                                       OncVirtualNetworkParser::ParseVPNValue);
    }
    case PROPERTY_INDEX_ONC_PROXY_SETTINGS:
      return ProcessProxySettings(parser, value, network);
    case PROPERTY_INDEX_ONC_REMOVE:
      // Removal is handled in ParseNetwork, and so is ignored here.
      return true;
    case PROPERTY_INDEX_TYPE: {
      // Update property with native value for type.
      std::string str =
          NativeNetworkParser::network_type_mapper()->GetKey(network->type());
      base::StringValue val(str);
      network->UpdatePropertyMap(PROPERTY_INDEX_TYPE, &val);
      return true;
    }
    case PROPERTY_INDEX_GUID:
      // Fall back to generic parser.
      return parser->ParseValue(index, value, network);
    case PROPERTY_INDEX_NAME:
      // shill doesn't allow setting name for non-VPN networks.
      if (network->type() != TYPE_VPN) {
        network->UpdatePropertyMap(PROPERTY_INDEX_NAME, NULL);
        return true;
      }

      // Fall back to generic parser.
      return parser->ParseValue(index, value, network);
    default:
      break;
  }
  return false;
}

// static
bool OncNetworkParser::CheckNetworkType(Network* network,
                                        ConnectionType expected,
                                        const std::string& onc_type) {
  if (expected != network->type()) {
    LOG(WARNING) << network->name() << ": "
                 << onc_type << " field unexpected for this type network";
    return false;
  }
  return true;
}

// static
ClientCertType OncNetworkParser::ParseClientCertType(
    const std::string& type) {
  static EnumMapper<ClientCertType>::Pair table[] = {
    { onc::certificate::kNone, CLIENT_CERT_TYPE_NONE },
    { onc::certificate::kRef, CLIENT_CERT_TYPE_REF },
    { onc::certificate::kPattern, CLIENT_CERT_TYPE_PATTERN },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ClientCertType>, parser,
      (table, arraysize(table), CLIENT_CERT_TYPE_NONE));
  return parser.Get(type);
}

// static
std::string OncNetworkParser::GetPkcs11IdFromCertGuid(const std::string& guid) {
  // We have to look up the GUID to find the PKCS#11 ID that is needed.
  net::CertificateList cert_list;
  onc::CertificateImporter::ListCertsWithNickname(guid, &cert_list);
  DCHECK_EQ(1ul, cert_list.size());
  if (cert_list.size() == 1)
    return x509_certificate_model::GetPkcs11Id(cert_list[0]->os_cert_handle());
  return std::string();
}

// static
bool OncNetworkParser::ProcessProxySettings(OncNetworkParser* parser,
                                            const base::Value& value,
                                            Network* network) {
  VLOG(1) << "Processing ProxySettings: " << ConvertValueToString(value);

  // Got "ProxySettings" field.  Immediately store the ProxySettings.Type
  // field value so that we can properly validate fields in the ProxySettings
  // object based on the type.
  const DictionaryValue* dict = NULL;
  CHECK(value.GetAsDictionary(&dict));
  std::string proxy_type_string;
  if (!dict->GetString(onc::proxy::kType, &proxy_type_string)) {
    VLOG(1) << network->name() << ": ProxySettings.Type is missing";
    return false;
  }
  Network::ProxyOncConfig& config = network->proxy_onc_config();
  config.type = ParseProxyType(proxy_type_string);

  // For Direct and WPAD, all other fields are ignored.
  // Otherwise, recursively parse the children of ProxySettings dictionary.
  if (config.type != PROXY_ONC_DIRECT && config.type != PROXY_ONC_WPAD) {
    if (!parser->ParseNestedObject(network,
                                   onc::kProxySettings,
                                   value,
                                   proxy_settings_signature,
                                   OncNetworkParser::ParseProxySettingsValue)) {
      return false;
    }
  }

  // Create ProxyConfigDictionary based on parsed values.
  scoped_ptr<DictionaryValue> proxy_dict;
  switch (config.type) {
    case PROXY_ONC_DIRECT: {
      proxy_dict.reset(ProxyConfigDictionary::CreateDirect());
      break;
    }
    case PROXY_ONC_WPAD: {
      proxy_dict.reset(ProxyConfigDictionary::CreateAutoDetect());
      break;
    }
    case PROXY_ONC_PAC: {
      if (config.pac_url.empty()) {
        VLOG(1) << "PAC field is required for this ProxySettings.Type";
        return false;
      }
      GURL url(config.pac_url);
      if (!url.is_valid()) {
        VLOG(1) << "PAC field is invalid for this ProxySettings.Type";
        return false;
      }
      proxy_dict.reset(ProxyConfigDictionary::CreatePacScript(url.spec(),
                                                              false));
      break;
    }
    case PROXY_ONC_MANUAL: {
      if (config.manual_spec.empty()) {
        VLOG(1) << "Manual field is required for this ProxySettings.Type";
        return false;
      }
      proxy_dict.reset(ProxyConfigDictionary::CreateFixedServers(
          config.manual_spec, config.bypass_rules));
      break;
    }
    default:
      break;
  }
  if (!proxy_dict.get())
    return false;

  // Serialize ProxyConfigDictionary into a string.
  std::string proxy_dict_str = ConvertValueToString(*proxy_dict.get());

  // Add ProxyConfig property to property map so that it will be updated in
  // shill in NetworkLibraryImplCros::CallConfigureService after all parsing
  // has completed.
  base::StringValue val(proxy_dict_str);
  network->UpdatePropertyMap(PROPERTY_INDEX_PROXY_CONFIG, &val);

  // If |network| is currently being connected to or it exists in memory,
  // shill will fire PropertyChanged notification in ConfigureService,
  // chromeos::ProxyConfigServiceImpl will get OnNetworkChanged notification
  // and, if necessary, activate |proxy_dict_str| on network stack and reflect
  // it in UI via "Change proxy settings" button.
  network->set_proxy_config(proxy_dict_str);
  VLOG(1) << "Parsed ProxyConfig: " << network->proxy_config();

  return true;
}

// static
bool OncNetworkParser::ParseProxySettingsValue(OncNetworkParser* parser,
                                               PropertyIndex index,
                                               const base::Value& value,
                                               Network* network) {
  Network::ProxyOncConfig& config = network->proxy_onc_config();
  switch (index) {
    case PROPERTY_INDEX_ONC_PROXY_PAC: {
      if (config.type == PROXY_ONC_PAC) {
        // This is a string specifying the url.
        config.pac_url = GetStringValue(value);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ONC_PROXY_MANUAL: {
      if (config.type == PROXY_ONC_MANUAL) {
        // Recursively parse the children of Manual dictionary.
        return parser->ParseNestedObject(network,
                                         onc::proxy::kManual,
                                         value,
                                         proxy_manual_signature,
                                         ParseProxyManualValue);
      }
      break;
    }
    case PROPERTY_INDEX_ONC_PROXY_EXCLUDE_DOMAINS: {
      if (config.type == PROXY_ONC_MANUAL) {
        // This is a list of rules, parse them into ProxyBypassRules.
        net::ProxyBypassRules rules;
        const ListValue* list = NULL;
        CHECK(value.GetAsList(&list));
        for (size_t i = 0; i < list->GetSize(); ++i) {
          std::string val;
          if (!list->GetString(i, &val))
            return false;
          rules.AddRuleFromString(val);
        }
        config.bypass_rules = rules.ToString();
        return true;
      }
      break;
    }
    default:
      break;
  }
  return true;
}

// static
ProxyOncType OncNetworkParser::ParseProxyType(const std::string& type) {
  static EnumMapper<ProxyOncType>::Pair table[] = {
    { onc::proxy::kDirect, PROXY_ONC_DIRECT },
    { onc::proxy::kWPAD, PROXY_ONC_WPAD },
    { onc::proxy::kPAC, PROXY_ONC_PAC },
    { onc::proxy::kManual, PROXY_ONC_MANUAL },
  };

  CR_DEFINE_STATIC_LOCAL(EnumMapper<ProxyOncType>, parser,
      (table, arraysize(table), PROXY_ONC_MAX));

  return parser.Get(type);
}

// static
bool OncNetworkParser::ParseProxyManualValue(OncNetworkParser* parser,
                                             PropertyIndex index,
                                             const base::Value& value,
                                             Network* network) {
  switch (index) {
    case PROPERTY_INDEX_ONC_PROXY_HTTP:
      return ParseProxyServer(index, value, "http", network);
      break;
    case PROPERTY_INDEX_ONC_PROXY_HTTPS:
      return ParseProxyServer(index, value, "https", network);
      break;
    case PROPERTY_INDEX_ONC_PROXY_FTP:
      return ParseProxyServer(index, value, "ftp", network);
      break;
    case PROPERTY_INDEX_ONC_PROXY_SOCKS:
      return ParseProxyServer(index, value, "socks", network);
      break;
    default:
      break;
  }
  return false;
}

// static
bool OncNetworkParser::ParseProxyServer(int property_index,
                                        const base::Value& value,
                                        const std::string& scheme,
                                        Network* network) {
  // Parse the ProxyLocation dictionary that specifies the proxy server.
  net::ProxyServer server = ParseProxyLocationValue(property_index, value);
  if (!server.is_valid())
    return false;

  // Append proxy server info to manual spec string.
  ProxyConfigServiceImpl::ProxyConfig::EncodeAndAppendProxyServer(scheme,
      server, &network->proxy_onc_config().manual_spec);
  return true;
}

// static
net::ProxyServer OncNetworkParser::ParseProxyLocationValue(
    int property_index,
    const base::Value& value) {
  // Extract the values of Host and Port keys in ProxyLocation dictionary.
  const DictionaryValue* dict = NULL;
  CHECK(value.GetAsDictionary(&dict));
  std::string host;
  int port = 0;
  if (!(dict->GetString(onc::proxy::kHost, &host) &&
        dict->GetInteger(onc::proxy::kPort, &port))) {
    return net::ProxyServer();
  }

  // Determine the scheme for the proxy server.
  net::ProxyServer::Scheme scheme = net::ProxyServer::SCHEME_HTTP;
  if (property_index == PROPERTY_INDEX_ONC_PROXY_SOCKS) {
    scheme = StartsWithASCII(host, "socks5://", false) ?
        net::ProxyServer::SCHEME_SOCKS5 : net::ProxyServer::SCHEME_SOCKS4;
  }

  // Process the Host and Port values into net::HostPortPair, and then
  // net::ProxyServer for the specific scheme.
  net::HostPortPair host_port(host, static_cast<uint16>(port));
  return net::ProxyServer(scheme, host_port);
}

// static
bool OncNetworkParser::ParseClientCertPattern(OncNetworkParser* parser,
                                              PropertyIndex index,
                                              const base::Value& value,
                                              Network* network) {
  // Ignore certificate patterns for device policy ONC so that an unmanaged user
  // won't have a certificate presented for them involuntarily.
  if (parser->onc_source() == onc::ONC_SOURCE_DEVICE_POLICY)
    return false;

  // Only WiFi and VPN have this type.
  if (network->type() != TYPE_WIFI &&
      network->type() != TYPE_VPN) {
    LOG(WARNING) << "Tried to parse a ClientCertPattern from something "
                 << "that wasn't a WiFi or VPN network.";
    return false;
  }


  switch (index) {
    case PROPERTY_INDEX_ONC_CERTIFICATE_PATTERN_ENROLLMENT_URI: {
      std::vector<std::string> resulting_list;
      if (!GetAsListOfStrings(value, &resulting_list))
        return false;
      CertificatePattern pattern = network->client_cert_pattern();
      pattern.set_enrollment_uri_list(resulting_list);
      network->set_client_cert_pattern(pattern);
      return true;
    }
    case PROPERTY_INDEX_ONC_CERTIFICATE_PATTERN_ISSUER_CA_REF: {
      std::vector<std::string> resulting_list;
      if (!GetAsListOfStrings(value, &resulting_list))
        return false;
      CertificatePattern pattern = network->client_cert_pattern();
      pattern.set_issuer_ca_ref_list(resulting_list);
      network->set_client_cert_pattern(pattern);
      return true;
    }
    case PROPERTY_INDEX_ONC_CERTIFICATE_PATTERN_ISSUER:
      return parser->ParseNestedObject(network,
                                       onc::certificate::kIssuer,
                                       value,
                                       issuer_subject_pattern_signature,
                                       ParseIssuerPattern);
    case PROPERTY_INDEX_ONC_CERTIFICATE_PATTERN_SUBJECT:
      return parser->ParseNestedObject(network,
                                       onc::certificate::kSubject,
                                       value,
                                       issuer_subject_pattern_signature,
                                       ParseSubjectPattern);
    default:
      break;
  }
  return false;
}

// static
bool OncNetworkParser::ParseIssuerPattern(OncNetworkParser* parser,
                                          PropertyIndex index,
                                          const base::Value& value,
                                          Network* network) {
  IssuerSubjectPattern pattern;
  if (ParseIssuerSubjectPattern(&pattern, parser, index, value, network)) {
    CertificatePattern cert_pattern = network->client_cert_pattern();
    cert_pattern.set_issuer(pattern);
    network->set_client_cert_pattern(cert_pattern);
    return true;
  }
  return false;
}

// static
bool OncNetworkParser::ParseSubjectPattern(OncNetworkParser* parser,
                                           PropertyIndex index,
                                           const base::Value& value,
                                           Network* network) {
  IssuerSubjectPattern pattern;
  if (ParseIssuerSubjectPattern(&pattern, parser, index, value, network)) {
    CertificatePattern cert_pattern = network->client_cert_pattern();
    cert_pattern.set_subject(pattern);
    network->set_client_cert_pattern(cert_pattern);
    return true;
  }
  return false;
}

// static
bool OncNetworkParser::ParseIssuerSubjectPattern(IssuerSubjectPattern* pattern,
                                                 OncNetworkParser* parser,
                                                 PropertyIndex index,
                                                 const base::Value& value,
                                                 Network* network) {
  // Only WiFi and VPN have this type.
  if (network->type() != TYPE_WIFI &&
      network->type() != TYPE_VPN) {
    LOG(WARNING) << "Tried to parse an IssuerSubjectPattern from something "
                 << "that wasn't a WiFi or VPN network.";
    return false;
  }
  std::string value_str;
  if (!value.GetAsString(&value_str))
    return false;

  bool result = false;
  switch (index) {
    case PROPERTY_INDEX_ISSUER_SUBJECT_PATTERN_COMMON_NAME:
      pattern->set_common_name(value_str);
      result = true;
      break;
    case PROPERTY_INDEX_ISSUER_SUBJECT_PATTERN_LOCALITY:
      pattern->set_locality(value_str);
      result = true;
      break;
    case PROPERTY_INDEX_ISSUER_SUBJECT_PATTERN_ORGANIZATION:
      pattern->set_organization(value_str);
      result = true;
      break;
    case PROPERTY_INDEX_ISSUER_SUBJECT_PATTERN_ORGANIZATIONAL_UNIT:
      pattern->set_organizational_unit(value_str);
      result = true;
      break;
    default:
      break;
  }
  return result;
}

// -------------------- OncEthernetNetworkParser --------------------

OncEthernetNetworkParser::OncEthernetNetworkParser() {}
OncEthernetNetworkParser::~OncEthernetNetworkParser() {}

bool OncEthernetNetworkParser::ParseEthernetValue(OncNetworkParser* parser,
                                                  PropertyIndex index,
                                                  const base::Value& value,
                                                  Network* network) {
  if (!CheckNetworkType(network, TYPE_ETHERNET, "Ethernet"))
    return false;
  // EthernetNetwork* ethernet_network = static_cast<EthernetNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_AUTHENTICATION:
      // TODO(chocobo): Handle authentication.
      return true;
    case PROPERTY_INDEX_EAP:
      // TODO(chocobo): Implement EAP authentication.
      return true;
    default:
      break;
  }
  return false;
}

// -------------------- OncWirelessNetworkParser --------------------

OncWirelessNetworkParser::OncWirelessNetworkParser() {}
OncWirelessNetworkParser::~OncWirelessNetworkParser() {}

// -------------------- OncWifiNetworkParser --------------------

OncWifiNetworkParser::OncWifiNetworkParser() {}
OncWifiNetworkParser::~OncWifiNetworkParser() {}

// static
bool OncWifiNetworkParser::ParseWifiValue(OncNetworkParser* parser,
                                          PropertyIndex index,
                                          const base::Value& value,
                                          Network* network) {
  if (!CheckNetworkType(network, TYPE_WIFI, "WiFi"))
    return false;
  WifiNetwork* wifi_network = static_cast<WifiNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_SSID:
      wifi_network->SetName(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_SECURITY: {
      ConnectionSecurity security = ParseSecurity(GetStringValue(value));
      wifi_network->set_encryption(security);
      // Also update property with native value for security.
      base::StringValue val(
          NativeNetworkParser::network_security_mapper()->GetKey(security));
      wifi_network->UpdatePropertyMap(index, &val);
      return true;
    }
    case PROPERTY_INDEX_PASSPHRASE:
      wifi_network->set_passphrase(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_IDENTITY:
      wifi_network->set_identity(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_EAP:
      parser->ParseNestedObject(wifi_network,
                                onc::wifi::kEAP,
                                value,
                                eap_signature,
                                ParseEAPValue);
      return true;
    case PROPERTY_INDEX_AUTO_CONNECT:
      network->set_auto_connect(GetBooleanValue(value));
      return true;
    case PROPERTY_INDEX_WIFI_HIDDEN_SSID:
      wifi_network->set_hidden_ssid(GetBooleanValue(value));
      return true;
    default:
      break;
  }
  return false;
}

// static
bool OncWifiNetworkParser::ParseEAPValue(OncNetworkParser* parser,
                                         PropertyIndex index,
                                         const base::Value& value,
                                         Network* network) {
  if (!CheckNetworkType(network, TYPE_WIFI, onc::wifi::kEAP))
    return false;
  WifiNetwork* wifi_network = static_cast<WifiNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_EAP_IDENTITY: {
      const std::string expanded_identity(
          GetUserExpandedValue(value, parser->onc_source()));
      wifi_network->set_eap_identity(expanded_identity);
      const StringValue expanded_identity_value(expanded_identity);
      wifi_network->UpdatePropertyMap(index, &expanded_identity_value);
      return true;
    }
    case PROPERTY_INDEX_EAP_METHOD: {
      EAPMethod eap_method = ParseEAPMethod(GetStringValue(value));
      wifi_network->set_eap_method(eap_method);
      // Also update property with native value for EAP method.
      base::StringValue val(
          NativeNetworkParser::network_eap_method_mapper()->GetKey(eap_method));
      wifi_network->UpdatePropertyMap(index, &val);
      return true;
    }
    case PROPERTY_INDEX_EAP_PHASE_2_AUTH: {
      EAPPhase2Auth eap_phase_2_auth = ParseEAPPhase2Auth(
          GetStringValue(value));
      wifi_network->set_eap_phase_2_auth(eap_phase_2_auth);
      // Also update property with native value for EAP phase 2 auth.
      base::StringValue val(
          NativeNetworkParser::network_eap_auth_mapper()->GetKey(
              eap_phase_2_auth));
      wifi_network->UpdatePropertyMap(index, &val);
      return true;
    }
    case PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY: {
      const std::string expanded_identity(
          GetUserExpandedValue(value, parser->onc_source()));
      wifi_network->set_eap_anonymous_identity(expanded_identity);
      const StringValue expanded_identity_value(expanded_identity);
      wifi_network->UpdatePropertyMap(index, &expanded_identity_value);
      return true;
    }
    case PROPERTY_INDEX_EAP_CERT_ID:
      wifi_network->set_eap_client_cert_pkcs11_id(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_EAP_CA_CERT_NSS:
      wifi_network->set_eap_server_ca_cert_nss_nickname(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_EAP_USE_SYSTEM_CAS:
      wifi_network->set_eap_use_system_cas(GetBooleanValue(value));
      return true;
    case PROPERTY_INDEX_EAP_PASSWORD:
      wifi_network->set_eap_passphrase(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_ONC_CLIENT_CERT_REF: {
      std::string cert_id = GetPkcs11IdFromCertGuid(GetStringValue(value));
      if (cert_id.empty())
        return false;
      wifi_network->set_eap_client_cert_pkcs11_id(cert_id);
      return true;
    }
    case PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN:
      return parser->ParseNestedObject(
            wifi_network,
            onc::eap::kClientCertPattern,
            value,
            certificate_pattern_signature,
            OncNetworkParser::ParseClientCertPattern);
    case PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE: {
      ClientCertType type = ParseClientCertType(GetStringValue(value));
      wifi_network->set_client_cert_type(type);
      return true;
    }
    case PROPERTY_INDEX_SAVE_CREDENTIALS:
      wifi_network->set_eap_save_credentials(GetBooleanValue(value));
      return true;
    default:
      break;
  }
  return false;
}

// static
ConnectionSecurity OncWifiNetworkParser::ParseSecurity(
    const std::string& security) {
  static EnumMapper<ConnectionSecurity>::Pair table[] = {
    { "None", SECURITY_NONE },
    { "WEP-PSK", SECURITY_WEP },
    { "WPA-PSK", SECURITY_PSK },
    { "WPA-EAP", SECURITY_8021X },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ConnectionSecurity>, parser,
      (table, arraysize(table), SECURITY_UNKNOWN));
  return parser.Get(security);
}

// static
EAPMethod OncWifiNetworkParser::ParseEAPMethod(const std::string& method) {
  static EnumMapper<EAPMethod>::Pair table[] = {
    { "PEAP", EAP_METHOD_PEAP },
    { "EAP-TLS", EAP_METHOD_TLS },
    { "EAP-TTLS", EAP_METHOD_TTLS },
    { "LEAP", EAP_METHOD_LEAP },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<EAPMethod>, parser,
      (table, arraysize(table), EAP_METHOD_UNKNOWN));
  return parser.Get(method);
}

// static
EAPPhase2Auth OncWifiNetworkParser::ParseEAPPhase2Auth(
    const std::string& auth) {
  static EnumMapper<EAPPhase2Auth>::Pair table[] = {
    { "MD5", EAP_PHASE_2_AUTH_MD5 },
    { "MSCHAPV2", EAP_PHASE_2_AUTH_MSCHAPV2 },
    { "MSCHAP", EAP_PHASE_2_AUTH_MSCHAP },
    { "PAP", EAP_PHASE_2_AUTH_PAP },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<EAPPhase2Auth>, parser,
      (table, arraysize(table), EAP_PHASE_2_AUTH_AUTO));
  return parser.Get(auth);
}

// -------------------- OncVirtualNetworkParser --------------------


OncVirtualNetworkParser::OncVirtualNetworkParser() {}
OncVirtualNetworkParser::~OncVirtualNetworkParser() {}

bool OncVirtualNetworkParser::UpdateNetworkFromInfo(
    const DictionaryValue& info,
    Network* network) {
  DCHECK_EQ(TYPE_VPN, network->type());
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  if (!OncNetworkParser::UpdateNetworkFromInfo(info, network))
    return false;
  VLOG(1) << "Updating VPN '" << virtual_network->name()
          << "': Server: " << virtual_network->server_hostname()
          << " Type: "
          << ProviderTypeToString(virtual_network->provider_type());
  return true;
}

// static
bool OncVirtualNetworkParser::ParseVPNValue(OncNetworkParser* parser,
                                            PropertyIndex index,
                                            const base::Value& value,
                                            Network* network) {
  if (!CheckNetworkType(network, TYPE_VPN, onc::kVPN))
    return false;
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_PROVIDER_HOST: {
      base::StringValue empty_value("");
      virtual_network->set_server_hostname(GetStringValue(value));
      // Shill requires a domain property which is unused.
      network->UpdatePropertyMap(PROPERTY_INDEX_VPN_DOMAIN, &empty_value);
      return true;
    }
    case PROPERTY_INDEX_ONC_IPSEC:
      if (virtual_network->provider_type() != PROVIDER_TYPE_L2TP_IPSEC_PSK &&
          virtual_network->provider_type() !=
          PROVIDER_TYPE_L2TP_IPSEC_USER_CERT) {
        VLOG(1) << "IPsec field not allowed with this VPN type";
        return false;
      }
      return parser->ParseNestedObject(network,
                                       onc::vpn::kIPsec,
                                       value,
                                       ipsec_signature,
                                       ParseIPsecValue);
    case PROPERTY_INDEX_ONC_L2TP:
      if (virtual_network->provider_type() != PROVIDER_TYPE_L2TP_IPSEC_PSK) {
        VLOG(1) << "L2TP field not allowed with this VPN type";
        return false;
      }
      return parser->ParseNestedObject(network,
                                       onc::vpn::kL2TP,
                                       value,
                                       l2tp_signature,
                                       ParseL2TPValue);
    case PROPERTY_INDEX_ONC_OPENVPN: {
      if (virtual_network->provider_type() != PROVIDER_TYPE_OPEN_VPN) {
        VLOG(1) << "OpenVPN field not allowed with this VPN type";
        return false;
      }
      // The following are needed by shill to set up the OpenVPN
      // management channel which every ONC OpenVPN configuration will
      // use.
      base::StringValue empty_value("");
      network->UpdatePropertyMap(PROPERTY_INDEX_OPEN_VPN_AUTHUSERPASS,
                                 &empty_value);
      network->UpdatePropertyMap(PROPERTY_INDEX_OPEN_VPN_MGMT_ENABLE,
                                 &empty_value);
      return parser->ParseNestedObject(network,
                                       onc::vpn::kOpenVPN,
                                       value,
                                       openvpn_signature,
                                       ParseOpenVPNValue);
    }
    case PROPERTY_INDEX_PROVIDER_TYPE: {
      // Update property with native value for provider type.
      ProviderType provider_type = GetCanonicalProviderType(
          virtual_network->provider_type());
      std::string str =
          NativeVirtualNetworkParser::provider_type_mapper()->GetKey(
              provider_type);
      base::StringValue val(str);
      network->UpdatePropertyMap(PROPERTY_INDEX_PROVIDER_TYPE, &val);
      return true;
    }
    default:
      break;
  }
  return false;
}

bool OncVirtualNetworkParser::ParseIPsecValue(OncNetworkParser* parser,
                                              PropertyIndex index,
                                              const base::Value& value,
                                              Network* network) {
  if (!CheckNetworkType(network, TYPE_VPN, onc::vpn::kIPsec))
    return false;
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_IPSEC_AUTHENTICATIONTYPE:
      virtual_network->set_provider_type(
          UpdateProviderTypeWithAuthType(virtual_network->provider_type(),
                                         GetStringValue(value)));
      return true;
    case PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS:
      virtual_network->set_ca_cert_nss(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_L2TPIPSEC_PSK:
      virtual_network->set_psk_passphrase(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_L2TPIPSEC_GROUP_NAME:
      virtual_network->set_group_name(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_SAVE_CREDENTIALS:
      // Note that the specification allows different settings for
      // IPsec credentials (PSK) and L2TP credentials (username and
      // password) but we merge them in our implementation as is required
      // with the current connection manager.
      virtual_network->set_save_credentials(GetBooleanValue(value));
      return true;
    case PROPERTY_INDEX_ONC_CLIENT_CERT_REF: {
      std::string cert_id = GetPkcs11IdFromCertGuid(GetStringValue(value));
      if (cert_id.empty())
        return false;
      virtual_network->set_client_cert_id(cert_id);
      return true;
    }
    case PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN: {
      return parser->ParseNestedObject(virtual_network,
                                       onc::vpn::kClientCertPattern,
                                       value,
                                       certificate_pattern_signature,
                                       ParseClientCertPattern);
    }
    case PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE: {
      ClientCertType type = ParseClientCertType(GetStringValue(value));
      virtual_network->set_client_cert_type(type);
      return true;
    }
    case PROPERTY_INDEX_IPSEC_IKEVERSION: {
      if (!value.IsType(TYPE_STRING)) {
        // Shill wants all provider properties to be strings.
        base::StringValue string_value(ConvertValueToString(value));
        virtual_network->UpdatePropertyMap(index, &string_value);
      }
      return true;
    }
    default:
      break;
  }
  return false;
}

// static
ProviderType OncVirtualNetworkParser::UpdateProviderTypeWithAuthType(
    ProviderType provider,
    const std::string& auth_type) {
  switch (provider) {
  case PROVIDER_TYPE_L2TP_IPSEC_PSK:
  case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
    if (auth_type == "Cert") {
      return PROVIDER_TYPE_L2TP_IPSEC_USER_CERT;
    } else {
      if (auth_type != "PSK") {
        VLOG(1) << "Unexpected authentication type " << auth_type;
        break;
      }
      return PROVIDER_TYPE_L2TP_IPSEC_PSK;
    }
  default:
    VLOG(1) << "Unexpected provider type with authentication type "
            << auth_type;
    break;
  }
  return provider;
}

// static
ProviderType OncVirtualNetworkParser::GetCanonicalProviderType(
    ProviderType provider_type) {
  if (provider_type == PROVIDER_TYPE_L2TP_IPSEC_USER_CERT)
    return PROVIDER_TYPE_L2TP_IPSEC_PSK;
  return provider_type;
}

// static
bool OncVirtualNetworkParser::ParseL2TPValue(OncNetworkParser* parser,
                                             PropertyIndex index,
                                             const base::Value& value,
                                             Network* network) {
  if (!CheckNetworkType(network, TYPE_VPN, "L2TP"))
    return false;
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_L2TPIPSEC_PASSWORD:
      virtual_network->set_user_passphrase(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_L2TPIPSEC_USER: {
      const std::string expanded_user(
          GetUserExpandedValue(value, parser->onc_source()));
      virtual_network->set_username(expanded_user);
      const StringValue expanded_user_value(expanded_user);
      virtual_network->UpdatePropertyMap(index, &expanded_user_value);
      return true;
    }
    case PROPERTY_INDEX_SAVE_CREDENTIALS:
      // Note that the specification allows different settings for
      // IPsec credentials (PSK) and L2TP credentials (username and
      // password) but we merge them in our implementation as is required
      // with the current connection manager.
      virtual_network->set_save_credentials(GetBooleanValue(value));
      return true;
    default:
      break;
  }
  return false;
}

// static
bool OncVirtualNetworkParser::ParseOpenVPNValue(OncNetworkParser* parser,
                                                PropertyIndex index,
                                                const base::Value& value,
                                                Network* network) {
  if (!CheckNetworkType(network, TYPE_VPN, "OpenVPN"))
    return false;
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_OPEN_VPN_PASSWORD:
      virtual_network->set_user_passphrase(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_OPEN_VPN_USER: {
      const std::string expanded_user(
          GetUserExpandedValue(value, parser->onc_source()));
      virtual_network->set_username(expanded_user);
      const StringValue expanded_user_value(expanded_user);
      virtual_network->UpdatePropertyMap(index, &expanded_user_value);
      return true;
    }
    case PROPERTY_INDEX_SAVE_CREDENTIALS:
      virtual_network->set_save_credentials(GetBooleanValue(value));
      return true;
    case PROPERTY_INDEX_OPEN_VPN_CACERT:
      virtual_network->set_ca_cert_nss(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_OPEN_VPN_REMOTECERTKU: {
      // ONC supports a list of these, but we shill supports only one
      // today.  So extract the first.
      const base::ListValue* value_list = NULL;
      value.GetAsList(&value_list);
      const base::Value* first_item = NULL;
      if (!value_list->Get(0, &first_item) ||
          !first_item->IsType(base::Value::TYPE_STRING)) {
        VLOG(1) << "RemoteCertKU must be non-empty list of strings";
        return false;
      }
      virtual_network->UpdatePropertyMap(index, first_item);
      return true;
    }
    case PROPERTY_INDEX_OPEN_VPN_AUTH:
    case PROPERTY_INDEX_OPEN_VPN_AUTHRETRY:
    case PROPERTY_INDEX_OPEN_VPN_AUTHNOCACHE:
    case PROPERTY_INDEX_OPEN_VPN_CERT:
    case PROPERTY_INDEX_OPEN_VPN_CIPHER:
    case PROPERTY_INDEX_OPEN_VPN_COMPLZO:
    case PROPERTY_INDEX_OPEN_VPN_COMPNOADAPT:
    case PROPERTY_INDEX_OPEN_VPN_KEYDIRECTION:
    case PROPERTY_INDEX_OPEN_VPN_NSCERTTYPE:
    case PROPERTY_INDEX_OPEN_VPN_PORT:
    case PROPERTY_INDEX_OPEN_VPN_PROTO:
    case PROPERTY_INDEX_OPEN_VPN_PUSHPEERINFO:
    case PROPERTY_INDEX_OPEN_VPN_REMOTECERTEKU:
    case PROPERTY_INDEX_OPEN_VPN_REMOTECERTTLS:
    case PROPERTY_INDEX_OPEN_VPN_RENEGSEC:
    case PROPERTY_INDEX_OPEN_VPN_SERVERPOLLTIMEOUT:
    case PROPERTY_INDEX_OPEN_VPN_SHAPER:
    case PROPERTY_INDEX_OPEN_VPN_STATICCHALLENGE:
    case PROPERTY_INDEX_OPEN_VPN_TLSAUTHCONTENTS:
    case PROPERTY_INDEX_OPEN_VPN_TLSREMOTE: {
      if (!value.IsType(TYPE_STRING)) {
        // Shill wants all provider properties to be strings.
        base::StringValue string_value(ConvertValueToString(value));
        virtual_network->UpdatePropertyMap(index, &string_value);
      }
      return true;
    }
    case PROPERTY_INDEX_ONC_CLIENT_CERT_REF: {
      std::string cert_id = GetPkcs11IdFromCertGuid(GetStringValue(value));
      if (cert_id.empty())
        return false;
      virtual_network->set_client_cert_id(cert_id);
      return true;
    }
    case PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN:
      return parser->ParseNestedObject(
          virtual_network,
          onc::eap::kClientCertPattern,
          value,
          certificate_pattern_signature,
          OncNetworkParser::ParseClientCertPattern);
    case PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE: {
      ClientCertType type = ParseClientCertType(GetStringValue(value));
      virtual_network->set_client_cert_type(type);
      return true;
    }

    default:
      break;
  }
  return false;
}

// static
ProviderType OncVirtualNetworkParser::ParseProviderType(
    const std::string& type) {
  static EnumMapper<ProviderType>::Pair table[] = {
    // We initially map to L2TP-IPsec PSK and then fix this up based
    // on the value of AuthenticationType.
    { "L2TP-IPsec", PROVIDER_TYPE_L2TP_IPSEC_PSK },
    { "OpenVPN", PROVIDER_TYPE_OPEN_VPN },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ProviderType>, parser,
      (table, arraysize(table), PROVIDER_TYPE_MAX));
  return parser.Get(type);
}

}  // namespace chromeos
