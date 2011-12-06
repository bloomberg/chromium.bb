// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_ONC_NETWORK_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_ONC_NETWORK_PARSER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"  // for OVERRIDE
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/network_parser.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace chromeos {

// This is a simple representation of the signature of an ONC typed
// field, used in validation and translation.  It could be extended
// to include more complex rules of when the field is required/optional,
// as well as to handle "enum" types, which are strings with a small
// static set of possible values.
struct OncValueSignature {
  const char* field;
  PropertyIndex index;
  base::Value::Type type;
};

// This is the network parser that parses the data from an Open Network
// Configuration (ONC) file. ONC files are in JSON format that describes
// networks. We will use this parser to parse the ONC JSON blob.
//
// For ONC file format, see: http://dev.chromium.org/chromium-os/
//     chromiumos-design-docs/open-network-configuration
class OncNetworkParser : public NetworkParser {
 public:
  typedef bool (*ParserPointer)(OncNetworkParser*,
                                PropertyIndex,
                                const base::Value&,
                                Network*);

  explicit OncNetworkParser(const std::string& onc_blob);
  virtual ~OncNetworkParser();
  static const EnumMapper<PropertyIndex>* property_mapper();

  // Returns the number of networks in the "NetworkConfigs" list.
  int GetNetworkConfigsSize() const;

  // Returns the number of certificates in the "Certificates" list.
  int GetCertificatesSize() const;

  // Call to create the network by parsing network config in the nth position.
  // (0-based). Returns NULL if there's a parse error or if n is out of range.
  Network* ParseNetwork(int n);

  // Call to parse and import the nth certificate in the certificate
  // list.  Returns false on failure.
  bool ParseCertificate(int n);

  virtual Network* CreateNetworkFromInfo(const std::string& service_path,
      const base::DictionaryValue& info) OVERRIDE;

  // Parses a nested ONC object with the given mapper and parser function.
  // If Value is not the proper type or there is an error in parsing
  // any individual field, VLOGs diagnostics, and returns false.
  bool ParseNestedObject(Network* network,
                         const std::string& onc_type,
                         const base::Value& value,
                         OncValueSignature* signature,
                         ParserPointer parser);

  const std::string& parse_error() const { return parse_error_; }

 protected:
  OncNetworkParser();

  virtual Network* CreateNewNetwork(ConnectionType type,
                                    const std::string& service_path) OVERRIDE;
  virtual ConnectionType ParseType(const std::string& type) OVERRIDE;
  virtual ConnectionType ParseTypeFromDictionary(
      const base::DictionaryValue& info) OVERRIDE;

  // Returns the type string from the dictionary of network values.
  std::string GetTypeFromDictionary(const base::DictionaryValue& info);

  // Returns the GUID string from the dictionary of network values.
  std::string GetGuidFromDictionary(const base::DictionaryValue& info);

  // Parse a field's value in the NetworkConfiguration object.
  static bool ParseNetworkConfigurationValue(OncNetworkParser* parser,
                                             PropertyIndex index,
                                             const base::Value& value,
                                             Network* network);

  // Issue a diagnostic and return false if a type mismatch is found.
  static bool CheckNetworkType(Network* network,
                               ConnectionType expected,
                               const std::string& onc_type);

 private:
  bool ParseServerOrCaCertificate(
    int cert_index,
    const std::string& cert_type,
    base::DictionaryValue* certificate);
  bool ParseClientCertificate(
    int cert_index,
    base::DictionaryValue* certificate);

  // Error message from the JSON parser, if applicable.
  std::string parse_error_;

  scoped_ptr<base::DictionaryValue> root_dict_;
  base::ListValue* network_configs_;
  base::ListValue* certificates_;

  DISALLOW_COPY_AND_ASSIGN(OncNetworkParser);
};

// Base for wireless networks.
class OncWirelessNetworkParser : public OncNetworkParser {
 public:
  OncWirelessNetworkParser();
  virtual ~OncWirelessNetworkParser();

 private:
  DISALLOW_COPY_AND_ASSIGN(OncWirelessNetworkParser);
};

// Class for parsing Wi-Fi networks.
class OncWifiNetworkParser : public OncWirelessNetworkParser {
 public:
  OncWifiNetworkParser();
  virtual ~OncWifiNetworkParser();
  static bool ParseWifiValue(OncNetworkParser* parser,
                             PropertyIndex index,
                             const base::Value& value,
                             Network* wifi_network);

 protected:
  static bool ParseEAPValue(OncNetworkParser* parser,
                            PropertyIndex index,
                            const base::Value& value,
                            Network* wifi_network);
  static ConnectionSecurity ParseSecurity(const std::string& security);
  static EAPMethod ParseEAPMethod(const std::string& method);
  static EAPPhase2Auth ParseEAPPhase2Auth(const std::string& auth);

 private:
  DISALLOW_COPY_AND_ASSIGN(OncWifiNetworkParser);
};

// Class for parsing virtual private networks.
class OncVirtualNetworkParser : public OncNetworkParser {
 public:
  OncVirtualNetworkParser();
  virtual ~OncVirtualNetworkParser();
  virtual bool UpdateNetworkFromInfo(
      const base::DictionaryValue& info, Network* network) OVERRIDE;
  static bool ParseVPNValue(OncNetworkParser* parser,
                            PropertyIndex index,
                            const base::Value& value,
                            Network* network);

  // network_library combines provider type and authentication type
  // (L2TP-IPsec with PSK vs with Certificates).  This function
  // takes a provider type and adds an authentication type to return
  // the updated provider type.
  static ProviderType UpdateProviderTypeWithAuthType(
      ProviderType provider,
      const std::string& auth_type);

  // This function takes a provider type (which includes authentication
  // type) and returns the canonical provider type from it.  For instance
  // for L2TP-IPsec, the PSK provider type is the canonical one.
  static ProviderType GetCanonicalProviderType(ProviderType provider_type);

  static ProviderType ParseProviderType(const std::string& type);

 protected:
  static bool ParseIPsecValue(OncNetworkParser* parser,
                              PropertyIndex index,
                              const base::Value& value,
                              Network* network);
  static bool ParseL2TPValue(OncNetworkParser* parser,
                             PropertyIndex index,
                             const base::Value& value,
                             Network* network);
  static bool ParseOpenVPNValue(OncNetworkParser* parser,
                                PropertyIndex index,
                                const base::Value& value,
                                Network* network);

 private:
  DISALLOW_COPY_AND_ASSIGN(OncVirtualNetworkParser);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_ONC_NETWORK_PARSER_H_
