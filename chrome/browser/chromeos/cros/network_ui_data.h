// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_UI_DATA_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_UI_DATA_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/certificate_pattern.h"
#include "chrome/browser/chromeos/cros/enum_mapper.h"
#include "chrome/browser/chromeos/cros/network_constants.h"
#include "chromeos/network/onc/onc_constants.h"

namespace chromeos {

class NetworkPropertyUIData;

// Helper for accessing and setting values in the network's UI data dictionary.
// Accessing values is done via static members that take the network as an
// argument. In order to fill a UI data dictionary, construct an instance, set
// up your data members, and call FillDictionary(). For example, if you have a
// |network|:
//
//      NetworkUIData ui_data;
//      ui_data.set_onc_source(onc::ONC_SOURCE_USER_IMPORT);
//      ui_data.FillDictionary(network->ui_data());
class NetworkUIData {
 public:
  NetworkUIData();
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

  // Fills in |dict| with the currently configured values. This will write the
  // keys appropriate for Network::ui_data() as defined below (kKeyXXX).
  void FillDictionary(base::DictionaryValue* dict) const;

  // Key for storing source of the ONC network, which is an integer according to
  // enum ONCSource.
  static const char kKeyONCSource[];

  // Key for storing certificate pattern for this network (if any).
  static const char kKeyCertificatePattern[];

  // Key for storing certificate type for this network (if any), which is one of
  // "pattern", "ref", or "none", according to ClientCertType.
  static const char kKeyCertificateType[];

 private:
  static EnumMapper<onc::ONCSource>& GetONCSourceMapper();
  static EnumMapper<ClientCertType>& GetClientCertMapper();

  CertificatePattern certificate_pattern_;
  onc::ONCSource onc_source_;
  ClientCertType certificate_type_;

  static const EnumMapper<onc::ONCSource>::Pair kONCSourceTable[];
  static const EnumMapper<ClientCertType>::Pair kClientCertTable[];
};

// Holds meta information for a network property: Whether the property is under
// policy control, if it is user-editable, and whether the policy-provided
// default value, if applicable.
class NetworkPropertyUIData {
 public:
  // Enum values indicating the entity controlling the property.
  enum Controller {
    // Property is managed by policy.
    CONTROLLER_POLICY,
    // The user controls the policy.
    CONTROLLER_USER,
  };

  // Initializes the object with CONTROLLER_USER and no default value.
  NetworkPropertyUIData();
  ~NetworkPropertyUIData();

  // Initializes the object by calling Reset() with the provided ui_data.
  explicit NetworkPropertyUIData(const NetworkUIData& ui_data);

  // Resets the property to the controller specified by the given |ui_data| and
  // clears the default value.
  void Reset(const NetworkUIData& ui_data);

  // Update the property object from dictionary, reading the key given by
  // |property_key|.
  void ParseOncProperty(const NetworkUIData& ui_data,
                        const base::DictionaryValue* onc,
                        const std::string& property_key);

  const base::Value* default_value() const { return default_value_.get(); }
  bool managed() const { return controller_ == CONTROLLER_POLICY; }
  bool recommended() const {
    return controller_ == CONTROLLER_USER && default_value_.get();
  }
  bool editable() const { return controller_ == CONTROLLER_USER; }

 private:
  Controller controller_;
  scoped_ptr<base::Value> default_value_;

  static const char kKeyController[];
  static const char kKeyDefaultValue[];

  // So it can access the kKeyXYZ constants.
  friend class NetworkUIDataTest;

  DISALLOW_COPY_AND_ASSIGN(NetworkPropertyUIData);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_UI_DATA_H_
