// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/onc_network_parser.h"

#include "base/base64.h"
#include "base/json/json_value_serializer.h"
#include "base/json/json_writer.h"  // for debug output only.
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/native_network_constants.h"
#include "chrome/browser/chromeos/cros/native_network_parser.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "net/base/cert_database.h"
#include "net/base/crypto_module.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// Local constants.
namespace {

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
  // TODO(crosbug.com/23673): Support Ethernet settings.
  { "GUID", PROPERTY_INDEX_GUID, TYPE_STRING },
  { "Name", PROPERTY_INDEX_NAME, TYPE_STRING },
  // TODO(crosbug.com/23674): Support ProxySettings.
  // TODO(crosbug.com/23604): Handle removing networks.
  { "Remove", PROPERTY_INDEX_ONC_REMOVE, TYPE_BOOLEAN },
  { "Type", PROPERTY_INDEX_TYPE, TYPE_STRING },
  { "WiFi", PROPERTY_INDEX_ONC_WIFI, TYPE_DICTIONARY },
  { "VPN", PROPERTY_INDEX_ONC_VPN, TYPE_DICTIONARY }
};

OncValueSignature wifi_signature[] = {
  { "AutoConnect", PROPERTY_INDEX_AUTO_CONNECT, TYPE_BOOLEAN },
  { "EAP", PROPERTY_INDEX_EAP, TYPE_DICTIONARY },
  { "HiddenSSID", PROPERTY_INDEX_HIDDEN_SSID, TYPE_BOOLEAN },
  { "Passphrase", PROPERTY_INDEX_PASSPHRASE, TYPE_STRING },
  { "ProxyURL", PROPERTY_INDEX_PROXY_CONFIG, TYPE_STRING },
  { "Security", PROPERTY_INDEX_SECURITY, TYPE_STRING },
  { "SSID", PROPERTY_INDEX_SSID, TYPE_STRING },
  { NULL }
};

OncValueSignature eap_signature[] = {
  { "AnonymousIdentity", PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY, TYPE_STRING },
  { "ClientCertPattern", PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN,
    TYPE_DICTIONARY },
  { "ClientCertRef", PROPERTY_INDEX_ONC_CLIENT_CERT_REF, TYPE_STRING },
  { "ClientCertType", PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE, TYPE_STRING },
  { "Identity", PROPERTY_INDEX_EAP_IDENTITY, TYPE_STRING },
  { "Inner", PROPERTY_INDEX_EAP_PHASE_2_AUTH, TYPE_STRING },
  { "Outer", PROPERTY_INDEX_EAP_METHOD, TYPE_STRING },
  { "Password", PROPERTY_INDEX_EAP_PASSWORD, TYPE_STRING },
  { "ServerCARef", PROPERTY_INDEX_EAP_CA_CERT, TYPE_STRING },
  { "UseSystemCAs", PROPERTY_INDEX_EAP_USE_SYSTEM_CAS, TYPE_BOOLEAN },
  { NULL }
};

OncValueSignature vpn_signature[] = {
  { "Host", PROPERTY_INDEX_PROVIDER_HOST, TYPE_STRING },
  { "IPsec", PROPERTY_INDEX_ONC_IPSEC, TYPE_DICTIONARY },
  { "L2TP", PROPERTY_INDEX_ONC_L2TP, TYPE_DICTIONARY },
  { "OpenVPN", PROPERTY_INDEX_ONC_OPENVPN, TYPE_DICTIONARY },
  { "Type", PROPERTY_INDEX_PROVIDER_TYPE, TYPE_STRING },
  { NULL }
};

OncValueSignature ipsec_signature[] = {
  { "AuthenticationType", PROPERTY_INDEX_IPSEC_AUTHENTICATIONTYPE,
    TYPE_STRING },
  { "Group", PROPERTY_INDEX_L2TPIPSEC_GROUP_NAME, TYPE_STRING },
  { "IKEVersion", PROPERTY_INDEX_IPSEC_IKEVERSION, TYPE_INTEGER },
  { "ClientCertPattern", PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN,
    TYPE_DICTIONARY },
  { "ClientCertRef", PROPERTY_INDEX_ONC_CLIENT_CERT_REF, TYPE_STRING },
  { "ClientCertType", PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE, TYPE_STRING },
  // Note: EAP and XAUTH not yet supported.
  { "PSK", PROPERTY_INDEX_L2TPIPSEC_PSK, TYPE_STRING },
  { "SaveCredentials", PROPERTY_INDEX_SAVE_CREDENTIALS, TYPE_BOOLEAN },
  { "ServerCARef", PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS, TYPE_STRING },
  { NULL }
};

OncValueSignature l2tp_signature[] = {
  { "Password", PROPERTY_INDEX_L2TPIPSEC_PASSWORD, TYPE_STRING },
  { "SaveCredentials", PROPERTY_INDEX_SAVE_CREDENTIALS, TYPE_BOOLEAN },
  { "Username", PROPERTY_INDEX_L2TPIPSEC_USER, TYPE_STRING },
  { NULL }
};

OncValueSignature openvpn_signature[] = {
  { "Auth", PROPERTY_INDEX_OPEN_VPN_AUTH, TYPE_STRING },
  { "AuthRetry", PROPERTY_INDEX_OPEN_VPN_AUTHRETRY, TYPE_STRING },
  { "AuthNoCache", PROPERTY_INDEX_OPEN_VPN_AUTHNOCACHE, TYPE_BOOLEAN },
  { "Cipher", PROPERTY_INDEX_OPEN_VPN_CIPHER, TYPE_STRING },
  { "ClientCertPattern", PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN,
    TYPE_DICTIONARY },
  { "ClientCertRef", PROPERTY_INDEX_ONC_CLIENT_CERT_REF, TYPE_STRING },
  { "ClientCertType", PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE, TYPE_STRING },
  { "CompLZO", PROPERTY_INDEX_OPEN_VPN_COMPLZO, TYPE_STRING },
  { "CompNoAdapt", PROPERTY_INDEX_OPEN_VPN_COMPNOADAPT, TYPE_BOOLEAN },
  { "KeyDirection", PROPERTY_INDEX_OPEN_VPN_KEYDIRECTION, TYPE_STRING },
  { "NsCertType", PROPERTY_INDEX_OPEN_VPN_NSCERTTYPE, TYPE_STRING },
  { "Password", PROPERTY_INDEX_OPEN_VPN_PASSWORD, TYPE_STRING },
  { "Port", PROPERTY_INDEX_OPEN_VPN_PORT, TYPE_INTEGER },
  { "Proto", PROPERTY_INDEX_OPEN_VPN_PROTO, TYPE_STRING },
  { "PushPeerInfo", PROPERTY_INDEX_OPEN_VPN_PUSHPEERINFO, TYPE_BOOLEAN },
  { "RemoteCertEKU", PROPERTY_INDEX_OPEN_VPN_REMOTECERTEKU, TYPE_STRING },
  { "RemoteCertKU", PROPERTY_INDEX_OPEN_VPN_REMOTECERTKU, TYPE_LIST },
  { "RemoteCertTLS", PROPERTY_INDEX_OPEN_VPN_REMOTECERTTLS, TYPE_STRING },
  { "RenegSec", PROPERTY_INDEX_OPEN_VPN_RENEGSEC, TYPE_INTEGER },
  { "SaveCredentials", PROPERTY_INDEX_SAVE_CREDENTIALS, TYPE_BOOLEAN },
  { "ServerCARef", PROPERTY_INDEX_OPEN_VPN_CACERT, TYPE_STRING },
  { "ServerCertRef", PROPERTY_INDEX_OPEN_VPN_CERT, TYPE_STRING },
  { "ServerPollTimeout", PROPERTY_INDEX_OPEN_VPN_SERVERPOLLTIMEOUT,
    TYPE_INTEGER },
  { "Shaper", PROPERTY_INDEX_OPEN_VPN_SHAPER, TYPE_INTEGER },
  { "StaticChallenge", PROPERTY_INDEX_OPEN_VPN_STATICCHALLENGE, TYPE_STRING },
  { "TLSAuthContents", PROPERTY_INDEX_OPEN_VPN_TLSAUTHCONTENTS, TYPE_STRING },
  { "TLSRemote", PROPERTY_INDEX_OPEN_VPN_TLSREMOTE, TYPE_STRING },
  { "Username", PROPERTY_INDEX_OPEN_VPN_USER, TYPE_STRING },
  { NULL }
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
  base::JSONWriter::Write(&value, false, &value_json);
  return value_json;
}

}  // namespace

// -------------------- OncNetworkParser --------------------

OncNetworkParser::OncNetworkParser(const std::string& onc_blob)
    : NetworkParser(get_onc_mapper()),
      network_configs_(NULL),
      certificates_(NULL) {
  VLOG(2) << __func__ << ": OncNetworkParser called on " << onc_blob;
  JSONStringValueSerializer deserializer(onc_blob);
  deserializer.set_allow_trailing_comma(true);
  scoped_ptr<base::Value> root(deserializer.Deserialize(NULL, &parse_error_));

  if (!root.get() || root->GetType() != base::Value::TYPE_DICTIONARY) {
    LOG(WARNING) << "OncNetworkParser received bad ONC file: " << parse_error_;
  } else {
    root_dict_.reset(static_cast<DictionaryValue*>(root.release()));
    // At least one of NetworkConfigurations or Certificates is required.
    bool has_network_configurations =
      root_dict_->GetList("NetworkConfigurations", &network_configs_);
    bool has_certificates =
      root_dict_->GetList("Certificates", &certificates_);
    VLOG(2) << "ONC file has " << GetNetworkConfigsSize() << " networks and "
            << GetCertificatesSize() << " certificates";
    if (!has_network_configurations || !has_certificates) {
      LOG(WARNING) << "ONC file has no NetworkConfigurations or Certificates.";
    }
  }
}

OncNetworkParser::OncNetworkParser()
    : NetworkParser(get_onc_mapper()),
      network_configs_(NULL),
      certificates_(NULL) {
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

int OncNetworkParser::GetCertificatesSize() const {
  return certificates_ ? certificates_->GetSize() : 0;
}

bool OncNetworkParser::ParseCertificate(int cert_index) {
  CHECK(certificates_);
  CHECK(static_cast<size_t>(cert_index) < certificates_->GetSize());
  CHECK(cert_index >= 0);
  base::DictionaryValue* certificate = NULL;
  certificates_->GetDictionary(cert_index, &certificate);
  CHECK(certificate);

  if (VLOG_IS_ON(2)) {
    std::string certificate_json;
    base::JSONWriter::Write(static_cast<base::Value*>(certificate),
                            true, &certificate_json);
    VLOG(2) << "Parsing certificate at index " << cert_index
            << ": " << certificate_json;
  }

  // Get out the attributes of the given cert.
  std::string guid;
  bool remove = false;
  if (!certificate->GetString("GUID", &guid) || guid.empty()) {
    LOG(WARNING) << "ONC File: certificate missing identifier at index"
                 << cert_index;
    return false;
  }

  if (!certificate->GetBoolean("Remove", &remove))
    remove = false;

  net::CertDatabase cert_database;
  if (remove)
    return cert_database.DeleteCertAndKeyByLabel(guid);

  // Not removing, so let's get the data we need to add this cert.
  std::string cert_type;
  certificate->GetString("Type", &cert_type);
  if (cert_type == "Server" || cert_type == "Authority") {
    return ParseServerOrCaCertificate(cert_index, cert_type, certificate);
  }
  if (cert_type == "Client") {
    return ParseClientCertificate(cert_index, certificate);
  }

  LOG(WARNING) << "ONC File: certificate of unknown type: " << cert_type
               << " at index " << cert_index;
  return false;
}

Network* OncNetworkParser::ParseNetwork(int n) {
  if (!network_configs_)
    return NULL;
  DictionaryValue* info = NULL;
  if (!network_configs_->GetDictionary(n, &info))
    return NULL;
  if (VLOG_IS_ON(2)) {
    std::string network_json;
    base::JSONWriter::Write(static_cast<base::Value*>(info),
                            true, &network_json);
    VLOG(2) << "Parsing network at index " << n
            << ": " << network_json;
  }

  return CreateNetworkFromInfo(std::string(), *info);
}

Network* OncNetworkParser::CreateNetworkFromInfo(
    const std::string& service_path,
    const DictionaryValue& info) {
  ConnectionType type = ParseTypeFromDictionary(info);
  if (type == TYPE_UNKNOWN)  // Return NULL if cannot parse network type.
    return NULL;
  scoped_ptr<Network> network(CreateNewNetwork(type, service_path));
  if (!ParseNestedObject(network.get(),
                         "NetworkConfiguration",
                         static_cast<const base::Value&>(info),
                         network_configuration_signature,
                         ParseNetworkConfigurationValue)) {
    LOG(WARNING) << "Network " << network->name() << " failed to parse.";
    return NULL;
  }
  if (VLOG_IS_ON(2)) {
    VLOG(2) << "Created Network '" << network->name()
            << "' from info. Path:" << service_path
            << " Type:" << ConnectionTypeToString(type);
  }
  return network.release();
}

Network* OncNetworkParser::CreateNewNetwork(
    ConnectionType type, const std::string& service_path) {
  Network* network = NetworkParser::CreateNewNetwork(type, service_path);
  if (network) {
    if (type == TYPE_WIFI)
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

bool OncNetworkParser::ParseServerOrCaCertificate(
    int cert_index,
    const std::string& cert_type,
    base::DictionaryValue* certificate) {
  net::CertDatabase cert_database;
  bool web_trust = false;
  base::ListValue* trust_list = NULL;
  if (certificate->GetList("Trust", &trust_list)) {
    for (size_t i = 0; i < trust_list->GetSize(); ++i) {
      std::string trust_type;
      if (!trust_list->GetString(i, &trust_type)) {
        LOG(WARNING) << "ONC File: certificate trust is invalid at index "
                     << cert_index;
        return false;
      }
      if (trust_type == "Web") {
        web_trust = true;
      } else {
        LOG(WARNING) << "ONC File: certificate contains unknown "
                     << "trust type: " << trust_type
                     << " at index " << cert_index;
        return false;
      }
    }
  }

  std::string x509_data;
  if (!certificate->GetString("X509", &x509_data) || x509_data.empty()) {
    LOG(WARNING) << "ONC File: certificate missing appropriate "
                 << "certificate data for type: " << cert_type
                 << " at index " << cert_index;
    return false;
  }

  std::string decoded_x509;
  if (!base::Base64Decode(x509_data, &decoded_x509)) {
    LOG(WARNING) << "Unable to base64 decode X509 data: \""
                 << x509_data << "\".";
    return false;
  }

  scoped_refptr<net::X509Certificate> x509_cert(
      net::X509Certificate::CreateFromBytes(decoded_x509.c_str(),
                                            decoded_x509.size()));
  if (!x509_cert.get()) {
    LOG(WARNING) << "Unable to create X509 certificate from bytes.";
    return false;
  }
  net::CertificateList cert_list;
  cert_list.push_back(x509_cert);
  net::CertDatabase::ImportCertFailureList failures;
  bool success = false;
  if (cert_type == "Server") {
    success = cert_database.ImportServerCert(cert_list, &failures);
  } else { // Authority cert
    net::CertDatabase::TrustBits trust = web_trust ?
                                         net::CertDatabase::TRUSTED_SSL :
                                         net::CertDatabase::UNTRUSTED;
    success = cert_database.ImportCACerts(cert_list, trust, &failures);
  }
  if (!failures.empty()) {
    LOG(WARNING) << "ONC File: Error ("
                 << net::ErrorToString(failures[0].net_error)
                 << ") importing " << cert_type << " certificate at index "
                 << cert_index;
    return false;
  }
  if (!success) {
    LOG(WARNING) << "ONC File: Unknown error importing " << cert_type
                 << " certificate at index " << cert_index;
    return false;
  }
  VLOG(2) << "Successfully imported server/ca certificate at index "
          << cert_index;
  return true;
}

bool OncNetworkParser::ParseClientCertificate(
    int cert_index,
    base::DictionaryValue* certificate) {
  net::CertDatabase cert_database;
  std::string pkcs12_data;
  if (!certificate->GetString("PKCS12", &pkcs12_data) ||
      pkcs12_data.empty()) {
    LOG(WARNING) << "ONC File: PKCS12 data is missing for Client "
                 << "certificate at index " << cert_index;
    return false;
  }

  std::string decoded_pkcs12;
  if (!base::Base64Decode(pkcs12_data, &decoded_pkcs12)) {
    LOG(WARNING) << "Unable to base64 decode PKCS#12 data: \""
                 << pkcs12_data << "\".";
    return false;
  }

  // Since this has a private key, always use the private module.
  scoped_refptr<net::CryptoModule> module(cert_database.GetPrivateModule());
  int result = cert_database.ImportFromPKCS12(
      module.get(), decoded_pkcs12, string16(), false);
  if (result != net::OK) {
    LOG(WARNING) << "ONC File: Unable to import Client certificate at index "
                 << cert_index
                 << " (error " << net::ErrorToString(result) << ").";
    return false;
  }
  VLOG(2) << "Successfully imported client certificate at index "
          << cert_index;
  return true;
}

bool OncNetworkParser::ParseNestedObject(Network* network,
                                         const std::string& onc_type,
                                         const base::Value& value,
                                         OncValueSignature* signature,
                                         ParserPointer parser) {
  bool any_errors = false;
  if (!value.IsType(base::Value::TYPE_DICTIONARY)) {
    VLOG(1) << network->name() << ": expected object of type " << onc_type;
    return false;
  }
  VLOG(2) << "Parsing nested object of type " << onc_type;
  const DictionaryValue* dict = NULL;
  value.GetAsDictionary(&dict);
  for (DictionaryValue::key_iterator iter = dict->begin_keys();
       iter != dict->end_keys(); ++iter) {
    const std::string& key = *iter;
    base::Value* inner_value = NULL;
    dict->GetWithoutPathExpansion(key, &inner_value);
    CHECK(inner_value != NULL);
    int field_index;
    for (field_index = 0; signature[field_index].field != NULL; ++field_index) {
      if (key == signature[field_index].field)
        break;
    }
    if (signature[field_index].field == NULL) {
      VLOG(1) << network->name() << ": unexpected field: "
              << key << ", in type: " << onc_type;
      any_errors = true;
      continue;
    }
    if (!inner_value->IsType(signature[field_index].type)) {
      VLOG(1) << network->name() << ": field with wrong type: " << key
              << ", actual type: " << inner_value->GetType()
              << ", expected type: " << signature[field_index].type;
      any_errors = true;
      continue;
    }
    PropertyIndex index = signature[field_index].index;
    // We need to UpdatePropertyMap now since parser might want to
    // change the mapped value.
    network->UpdatePropertyMap(index, *inner_value);
    if (!parser(this, index, *inner_value, network)) {
      VLOG(1) << network->name() << ": field not parsed: " << key;
      any_errors = true;
      continue;
    }
    if (VLOG_IS_ON(2)) {
      std::string value_json;
      base::JSONWriter::Write(inner_value, true, &value_json);
      VLOG(2) << network->name() << ": Successfully parsed [" << key
              << "(" << index << ")] = " << value_json;
    }
  }
  return !any_errors;
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
bool OncNetworkParser::ParseNetworkConfigurationValue(
    OncNetworkParser* parser,
    PropertyIndex index,
    const base::Value& value,
    Network* network) {
  switch (index) {
    case PROPERTY_INDEX_ONC_WIFI: {
      return parser->ParseNestedObject(network,
                                       "WiFi",
                                       value,
                                       wifi_signature,
                                       OncWifiNetworkParser::ParseWifiValue);
    }
    case PROPERTY_INDEX_ONC_VPN: {
      if (!CheckNetworkType(network, TYPE_VPN, "VPN"))
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
                                       "VPN",
                                       value,
                                       vpn_signature,
                                       OncVirtualNetworkParser::ParseVPNValue);
      return true;
    }
    case PROPERTY_INDEX_ONC_REMOVE:
      VLOG(1) << network->name() << ": Remove field not yet implemented";
      return false;
    case PROPERTY_INDEX_TYPE: {
      // Update property with native value for type.
      std::string str =
        NativeNetworkParser::network_type_mapper()->GetKey(network->type());
      scoped_ptr<StringValue> val(Value::CreateStringValue(str));
      network->UpdatePropertyMap(PROPERTY_INDEX_TYPE, *val.get());
      return true;
    }
    case PROPERTY_INDEX_GUID:
    case PROPERTY_INDEX_NAME:
      // Fall back to generic parser for these.
      return parser->ParseValue(index, value, network);
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
      std::string str =
          NativeNetworkParser::network_security_mapper()->GetKey(security);
      scoped_ptr<StringValue> val(Value::CreateStringValue(str));
      wifi_network->UpdatePropertyMap(index, *val.get());
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
                                "EAP",
                                value,
                                eap_signature,
                                ParseEAPValue);
      return true;
    case PROPERTY_INDEX_AUTO_CONNECT:
      network->set_auto_connect(GetBooleanValue(value));
      return true;
    case PROPERTY_INDEX_HIDDEN_SSID:
      // Pass this through to connection manager as is.
      return true;
    default:
      break;
  }
  return false;
}

// static
bool OncWifiNetworkParser::ParseEAPValue(OncNetworkParser*,
                                         PropertyIndex index,
                                         const base::Value& value,
                                         Network* network) {
  if (!CheckNetworkType(network, TYPE_WIFI, "EAP"))
    return false;
  WifiNetwork* wifi_network = static_cast<WifiNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_EAP_IDENTITY:
      // TODO(crosbug.com/23751): Support string expansion.
      wifi_network->set_eap_identity(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_EAP_METHOD: {
      EAPMethod eap_method = ParseEAPMethod(GetStringValue(value));
      wifi_network->set_eap_method(eap_method);
      // Also update property with native value for EAP method.
      std::string str =
          NativeNetworkParser::network_eap_method_mapper()->GetKey(eap_method);
      scoped_ptr<StringValue> val(Value::CreateStringValue(str));
      wifi_network->UpdatePropertyMap(index, *val.get());
      return true;
    }
    case PROPERTY_INDEX_EAP_PHASE_2_AUTH: {
      EAPPhase2Auth eap_phase_2_auth = ParseEAPPhase2Auth(
          GetStringValue(value));
      wifi_network->set_eap_phase_2_auth(eap_phase_2_auth);
      // Also update property with native value for EAP phase 2 auth.
      std::string str = NativeNetworkParser::network_eap_auth_mapper()->GetKey(
          eap_phase_2_auth);
      scoped_ptr<StringValue> val(Value::CreateStringValue(str));
      wifi_network->UpdatePropertyMap(index, *val.get());
      return true;
    }
    case PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY:
      wifi_network->set_eap_anonymous_identity(GetStringValue(value));
      return true;
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
    case PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN:
    case PROPERTY_INDEX_ONC_CLIENT_CERT_REF:
    case PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE:
      // TODO(crosbug.com/19409): Support certificate patterns.
      // Ignore for now.
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
    { "WPA-PSK", SECURITY_WPA },
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
  if (!CheckNetworkType(network, TYPE_VPN, "VPN"))
    return false;
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_PROVIDER_HOST: {
      virtual_network->set_server_hostname(GetStringValue(value));
      // Flimflam requires a domain property which is unused.
      network->UpdatePropertyMap(PROPERTY_INDEX_VPN_DOMAIN,
                                 base::StringValue(""));
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
                                       "IPsec",
                                       value,
                                       ipsec_signature,
                                       ParseIPsecValue);
    case PROPERTY_INDEX_ONC_L2TP:
      if (virtual_network->provider_type() != PROVIDER_TYPE_L2TP_IPSEC_PSK) {
        VLOG(1) << "L2TP field not allowed with this VPN type";
        return false;
      }
      return parser->ParseNestedObject(network,
                                       "L2TP",
                                       value,
                                       l2tp_signature,
                                       ParseL2TPValue);
    case PROPERTY_INDEX_ONC_OPENVPN:
      if (virtual_network->provider_type() != PROVIDER_TYPE_OPEN_VPN) {
        VLOG(1) << "OpenVPN field not allowed with this VPN type";
        return false;
      }
      // The following are needed by flimflam to set up the OpenVPN
      // management channel which every ONC OpenVPN configuration will
      // use.
      network->UpdatePropertyMap(PROPERTY_INDEX_OPEN_VPN_AUTHUSERPASS,
                                 StringValue(""));
      network->UpdatePropertyMap(PROPERTY_INDEX_OPEN_VPN_MGMT_ENABLE,
                                 StringValue(""));
      return parser->ParseNestedObject(network,
                                       "OpenVPN",
                                       value,
                                       openvpn_signature,
                                       ParseOpenVPNValue);
    case PROPERTY_INDEX_PROVIDER_TYPE: {
      // Update property with native value for provider type.
      ProviderType provider_type = GetCanonicalProviderType(
          virtual_network->provider_type());
      std::string str =
        NativeVirtualNetworkParser::provider_type_mapper()->GetKey(
            provider_type);
      scoped_ptr<StringValue> val(Value::CreateStringValue(str));
      network->UpdatePropertyMap(PROPERTY_INDEX_PROVIDER_TYPE, *val.get());
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
  if (!CheckNetworkType(network, TYPE_VPN, "IPsec"))
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
    case PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN:
    case PROPERTY_INDEX_ONC_CLIENT_CERT_REF:
    case PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE:
      // TODO(crosbug.com/19409): Support certificate patterns.
      // Ignore for now.
      return true;
    case PROPERTY_INDEX_IPSEC_IKEVERSION:
      if (!value.IsType(TYPE_STRING)) {
        // Flimflam wants all provider properties to be strings.
        virtual_network->UpdatePropertyMap(
            index,
            StringValue(ConvertValueToString(value)));
      }
      return true;
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
bool OncVirtualNetworkParser::ParseL2TPValue(OncNetworkParser*,
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
    case PROPERTY_INDEX_L2TPIPSEC_USER:
      // TODO(crosbug.com/23751): Support string expansion.
      virtual_network->set_username(GetStringValue(value));
      return true;
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
bool OncVirtualNetworkParser::ParseOpenVPNValue(OncNetworkParser*,
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
    case PROPERTY_INDEX_OPEN_VPN_USER:
      // TODO(crosbug.com/23751): Support string expansion.
      virtual_network->set_username(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_SAVE_CREDENTIALS:
      virtual_network->set_save_credentials(GetBooleanValue(value));
      return true;
    case PROPERTY_INDEX_OPEN_VPN_CACERT:
      virtual_network->set_ca_cert_nss(GetStringValue(value));
      return true;
    case PROPERTY_INDEX_OPEN_VPN_REMOTECERTKU: {
      // ONC supports a list of these, but we flimflam supports only one
      // today.  So extract the first.
      const base::ListValue* value_list = NULL;
      value.GetAsList(&value_list);
      base::Value* first_item = NULL;
      if (!value_list->Get(0, &first_item) ||
          !first_item->IsType(base::Value::TYPE_STRING)) {
        VLOG(1) << "RemoteCertKU must be non-empty list of strings";
        return false;
      }
      virtual_network->UpdatePropertyMap(index, *first_item);
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
    case PROPERTY_INDEX_OPEN_VPN_TLSREMOTE:
      if (!value.IsType(TYPE_STRING)) {
        // Flimflam wants all provider properties to be strings.
        virtual_network->UpdatePropertyMap(
            index,
            StringValue(ConvertValueToString(value)));
      }
      return true;
    case PROPERTY_INDEX_ONC_CLIENT_CERT_PATTERN:
    case PROPERTY_INDEX_ONC_CLIENT_CERT_REF:
    case PROPERTY_INDEX_ONC_CLIENT_CERT_TYPE:
      // TODO(crosbug.com/19409): Support certificate patterns.
      // Ignore for now.
      return true;

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
