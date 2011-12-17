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
#include "chrome/browser/chromeos/cros/enum_mapper.h"

namespace chromeos {

class NetworkPropertyUIData;

// Helper for accessing and setting values in the network's UI data dictionary.
// Accessing values is done via static members that take the network as an
// argument. In order to fill a UI data dictionary, construct an instance, set
// up your data members, and call FillDictionary(). For example, if you have a
// |network|:
//
//      NetworkUIData ui_data;
//      ui_data.set_onc_source(NetworkUIData::ONC_SOURCE_USER_IMPORT);
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

  // Fills in |dict| with the currently configured values. This will write the
  // keys appropriate for Network::ui_data() as defined below (kKeyXXX).
  void FillDictionary(base::DictionaryValue* dict) const;

  // Get the ONC source for a network.
  static ONCSource GetONCSource(const base::DictionaryValue* ui_data);

  // Check whether a network is managed by policy.
  static bool IsManaged(const base::DictionaryValue* ui_data);

  // Source of the ONC network. This is an integer according to enum ONCSource.
  static const char kKeyONCSource[];

 private:
  static EnumMapper<ONCSource>& GetONCSourceMapper();

  ONCSource onc_source_;

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

  // Initializes the object by calling Reset() with the provided ui_data.
  explicit NetworkPropertyUIData(const base::DictionaryValue* ui_data);

  // Initializes the object with the given values. |default_value| may be NULL
  // to specify no default value is present. Takes ownership of |default_value|.
  NetworkPropertyUIData(Controller controller, base::Value* default_value);

  // Resets the property to the controller specified by the given |ui_data| and
  // clears the default value.
  void Reset(const base::DictionaryValue* ui_data);

  // Update the property object from dictionary, reading the key given by
  // |property_key|.
  void ParseOncProperty(const base::DictionaryValue* ui_data,
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
