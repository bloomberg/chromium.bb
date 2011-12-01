// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_UI_DATA_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_UI_DATA_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/network_parser.h"

namespace chromeos {

class Network;
class NetworkPropertyUIData;

// Helper for accessing and setting values in the network's UI data dictionary.
// Accessing values is done via static members that take the network as an
// argument. In order to fill a UI data dictionary, construct an instance, set
// up your data members, and call FillDictionary(). For example, if you have a
// |network|:
//
//      NetworkUIData ui_data;
//      ui_data.set_onc_source(NetworkUIData::ONC_SOURCE_USER_IMPORT);
//      NetworkPropertyUIData auto_connect_property(
//              NetworkPropertyUIData::CONTROLLER_USER,
//              base::Value::CreateBooleanValue(true));
//      ui_data.SetProperty(NetworkUIData::kPropertyAutoConnect,
//                          auto_connect_property);
//      ui_data.FillDictionary(network->ui_data());
class NetworkUIData {
 public:
  // Indicates from which source an ONC blob comes from.
  enum ONCSource {
    ONC_SOURCE_NONE,
    ONC_SOURCE_USER_IMPORT,
    ONC_SOURCE_DEVICE_POLICY,
    ONC_SOURCE_USER_POLICY,
  };

  // Constructs a new dictionary builder.
  NetworkUIData();
  ~NetworkUIData();

  // Sets the ONC source.
  void set_onc_source(ONCSource onc_source) { onc_source_ = onc_source; }

  // Fills in metadata for a property.
  void SetProperty(const char* property_key,
                   const NetworkPropertyUIData& ui_data);

  // Fills in |dict| with the currently configured values. This will write the
  // keys appropriate for Network::ui_data() as defined below (kKeyXXX).
  void FillDictionary(DictionaryValue* dict) const;

  // Get the ONC source for a network.
  static ONCSource GetONCSource(const Network* network);

  // Check whether a network is managed by policy.
  static bool IsManaged(const Network* network);

  // Source of the ONC network. This is an integer according to enum ONCSource.
  static const char kKeyONCSource[];

  // Per-property meta data. This is handled by NetworkPropertyUIData.
  static const char kKeyProperties[];

  // Property names for per-property dat
  static const char kPropertyAutoConnect[];
  static const char kPropertyPreferred[];
  static const char kPropertyPassphrase[];
  static const char kPropertySaveCredentials[];

  static const char kPropertyVPNCaCertNss[];
  static const char kPropertyVPNPskPassphrase[];
  static const char kPropertyVPNClientCertId[];
  static const char kPropertyVPNUsername[];
  static const char kPropertyVPNUserPassphrase[];
  static const char kPropertyVPNGroupName[];

  static const char kPropertyEAPMethod[];
  static const char kPropertyEAPPhase2Auth[];
  static const char kPropertyEAPServerCaCertNssNickname[];
  static const char kPropertyEAPClientCertPkcs11Id[];
  static const char kPropertyEAPUseSystemCAs[];
  static const char kPropertyEAPIdentity[];
  static const char kPropertyEAPAnonymousIdentity[];
  static const char kPropertyEAPPassphrase[];

 private:
  static EnumMapper<ONCSource>& GetONCSourceMapper();

  ONCSource onc_source_;
  DictionaryValue properties_;

  static const EnumMapper<NetworkUIData::ONCSource>::Pair kONCSourceTable[];

  DISALLOW_COPY_AND_ASSIGN(NetworkUIData);
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

  // Initializes the object with the given values. |default_value| may be NULL
  // to specify no default value is present. Takes ownership of |default_value|.
  NetworkPropertyUIData(Controller controller, base::Value* default_value);

  // Initializes the object and calls UpdateFromNetwork.
  NetworkPropertyUIData(const Network* network, const char* property_key);

  // Updates the object from the network-level UI data dictionary.
  // |property_key| may be null, in which case the there'll be no default value
  // and |controller| will be set to CONTROLLER_POLICY iff the network is
  // managed.
  void UpdateFromNetwork(const Network* network, const char* property_key);
  // Builds a dictionary for storing in the network-level UI data dictionary.
  // Ownership is transferred to the caller.
  DictionaryValue* BuildDictionary() const;

  const base::Value* default_value() const { return default_value_.get(); }
  bool managed() const { return controller_ == CONTROLLER_POLICY; }
  bool recommended() const {
    return controller_ == CONTROLLER_USER && default_value_.get();
  }
  bool editable() const { return controller_ == CONTROLLER_USER; }

 private:
  static EnumMapper<Controller>& GetControllerMapper();

  Controller controller_;
  scoped_ptr<base::Value> default_value_;

  static const char kKeyController[];
  static const char kKeyDefaultValue[];

  static const EnumMapper<NetworkPropertyUIData::Controller>::Pair
    kControllerTable[];

  // So it can access the kKeyXYZ constants.
  friend class NetworkUIDataTest;

  DISALLOW_COPY_AND_ASSIGN(NetworkPropertyUIData);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_UI_DATA_H_
