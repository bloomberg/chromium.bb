// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_ONC_NETWORK_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_ONC_NETWORK_PARSER_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"  // for OVERRIDE
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/network_parser.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace chromeos {

// This is the network parser that parses the data from an Open Network
// Configuration (ONC) file. ONC files are in JSON format that describes
// networks. We will use this parser to parse the ONC JSON blob.
//
// For ONC file format, see: http://dev.chromium.org/chromium-os/
//     chromiumos-design-docs/open-network-configuration
class OncNetworkParser : public NetworkParser {
 public:
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
  virtual bool ParseValue(PropertyIndex index,
                          const base::Value& value,
                          Network* network) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(OncWirelessNetworkParser);
};

class OncWifiNetworkParser : public OncWirelessNetworkParser {
 public:
  OncWifiNetworkParser();
  virtual ~OncWifiNetworkParser();
  virtual bool ParseValue(PropertyIndex index,
                          const base::Value& value,
                          Network* network) OVERRIDE;
 protected:
  bool ParseEAPValue(PropertyIndex index,
                     const base::Value& value,
                     WifiNetwork* wifi_network);
  ConnectionSecurity ParseSecurity(const std::string& security);
  EAPMethod ParseEAPMethod(const std::string& method);
  EAPPhase2Auth ParseEAPPhase2Auth(const std::string& auth);
 private:
  DISALLOW_COPY_AND_ASSIGN(OncWifiNetworkParser);
};

class OncVirtualNetworkParser : public OncNetworkParser {
 public:
  OncVirtualNetworkParser();
  virtual ~OncVirtualNetworkParser();
  virtual bool ParseValue(PropertyIndex index,
                          const base::Value& value,
                          Network* network) OVERRIDE;
  virtual bool UpdateNetworkFromInfo(
      const base::DictionaryValue& info, Network* network) OVERRIDE;
 protected:
  bool ParseProviderValue(PropertyIndex index,
                          const base::Value& value,
                          VirtualNetwork* network);
  ProviderType ParseProviderType(const std::string& type);
 private:
  DISALLOW_COPY_AND_ASSIGN(OncVirtualNetworkParser);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_ONC_NETWORK_PARSER_H_
