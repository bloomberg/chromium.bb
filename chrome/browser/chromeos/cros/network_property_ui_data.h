// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_PROPERTY_UI_DATA_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_PROPERTY_UI_DATA_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromeos {

class NetworkUIData;

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

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_PROPERTY_UI_DATA_H_
