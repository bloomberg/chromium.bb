// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_UI_DATA_H_
#define CHROMEOS_NETWORK_NETWORK_UI_DATA_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/certificate_pattern.h"
#include "chromeos/network/onc/onc_constants.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

enum ClientCertType {
  CLIENT_CERT_TYPE_NONE    = 0,
  CLIENT_CERT_TYPE_REF     = 1,
  CLIENT_CERT_TYPE_PATTERN = 2
};

// Helper for accessing and setting values in the network's UI data dictionary.
// Accessing values is done via static members that take the network as an
// argument. In order to fill a UI data dictionary, construct an instance, set
// up your data members, and call FillDictionary(). For example, if you have a
// |network|:
//
//      NetworkUIData ui_data;
//      ui_data.set_onc_source(onc::ONC_SOURCE_USER_IMPORT);
//      ui_data.FillDictionary(network->ui_data());
class CHROMEOS_EXPORT NetworkUIData {
 public:
  NetworkUIData();
  NetworkUIData(const NetworkUIData& other);
  NetworkUIData& operator=(const NetworkUIData& other);
  explicit NetworkUIData(const base::DictionaryValue& dict);
  ~NetworkUIData();

  void set_onc_source(onc::ONCSource onc_source) { onc_source_ = onc_source; }
  onc::ONCSource onc_source() const { return onc_source_; }

  void set_certificate_pattern(const CertificatePattern& pattern) {
    certificate_pattern_ = pattern;
  }
  const CertificatePattern& certificate_pattern() const {
    return certificate_pattern_;
  }
  void set_certificate_type(ClientCertType type) {
    certificate_type_ = type;
  }
  ClientCertType certificate_type() const {
    return certificate_type_;
  }
  bool is_managed() const {
    return onc_source_ == onc::ONC_SOURCE_DEVICE_POLICY ||
        onc_source_ == onc::ONC_SOURCE_USER_POLICY;
  }
  const base::DictionaryValue* user_settings() const {
    return user_settings_.get();
  }
  void set_user_settings(scoped_ptr<base::DictionaryValue> dict) {
    user_settings_ = dict.Pass();
  }
  const std::string& policy_guid() const {
    return policy_guid_;
  }
  void set_policy_guid(const std::string& guid) {
    policy_guid_ = guid;
  }

  // Fills in |dict| with the currently configured values. This will write the
  // keys appropriate for Network::ui_data() as defined below (kKeyXXX).
  void FillDictionary(base::DictionaryValue* dict) const;

  // Key for storing source of the ONC network.
  static const char kKeyONCSource[];

  // Key for storing the certificate pattern.
  static const char kKeyCertificatePattern[];

  // Key for storing the certificate type.
  static const char kKeyCertificateType[];

  // Key for storing the user settings.
  static const char kKeyUserSettings[];

 private:
  CertificatePattern certificate_pattern_;
  onc::ONCSource onc_source_;
  ClientCertType certificate_type_;
  scoped_ptr<base::DictionaryValue> user_settings_;
  std::string policy_guid_;
};

// Creates a NetworkUIData object from |onc_network|, which has to be a valid
// ONC NetworkConfiguration dictionary.
//
// This function is used to create the "UIData" field of the Shill
// configuration.
CHROMEOS_EXPORT scoped_ptr<NetworkUIData> CreateUIDataFromONC(
    onc::ONCSource onc_source,
    const base::DictionaryValue& onc_network);

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_UI_DATA_H_
