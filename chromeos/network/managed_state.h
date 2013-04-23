// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MANAGED_STATE_H_
#define CHROMEOS_NETWORK_MANAGED_STATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/chromeos_export.h"

namespace base {
class Value;
}

namespace chromeos {

class DeviceState;
class NetworkState;

// Base class for states managed by NetworkStateManger which are associated
// with a Shill path (e.g. service path or device path).
class ManagedState {
 public:
  enum ManagedType {
    MANAGED_TYPE_NETWORK,
    MANAGED_TYPE_DEVICE
  };

  virtual ~ManagedState();

  // This will construct and return a new instance of the approprate class
  // based on |type|.
  static ManagedState* Create(ManagedType type, const std::string& path);

  // Returns the specific class pointer if this is the correct type, or
  // NULL if it is not.
  NetworkState* AsNetworkState();
  DeviceState* AsDeviceState();

  // Called by NetworkStateHandler when a property changes. Returns true if
  // the property was recognized and parsed successfully.
  virtual bool PropertyChanged(const std::string& key,
                               const base::Value& value) = 0;

  // Called by NetworkStateHandler after all calls to PropertyChanged for the
  // initial set of properties. Used to update state requiring multiple
  // parsed properties, e.g. name from hex_ssid in NetworkState.
  virtual void InitialPropertiesReceived();

  const ManagedType managed_type() const { return managed_type_; }
  const std::string& path() const { return path_; }
  const std::string& name() const { return name_; }
  const std::string& type() const { return type_; }
  bool is_observed() const { return is_observed_; }
  void set_is_observed(bool is_observed) { is_observed_ = is_observed; }

 protected:
  ManagedState(ManagedType type, const std::string& path);

  // Parses common property keys (name, type).
  bool ManagedStatePropertyChanged(const std::string& key,
                                   const base::Value& value);

  // Helper methods that log warnings and return false if parsing failed.
  bool GetBooleanValue(const std::string& key,
                       const base::Value& value,
                       bool* out_value);
  bool GetIntegerValue(const std::string& key,
                       const base::Value& value,
                       int* out_value);
  bool GetStringValue(const std::string& key,
                      const base::Value& value,
                      std::string* out_value);

  void set_name(const std::string& name) { name_ = name; }

 private:
  friend class NetworkChangeNotifierChromeosUpdateTest;

  ManagedType managed_type_;

  // The path (e.g. service path or device path) of the managed state object.
  std::string path_;

  // Common properties shared by all managed state objects.
  std::string name_;  // flimflam::kNameProperty
  std::string type_;  // flimflam::kTypeProperty

  // Tracks when the state is being observed.
  bool is_observed_;

  DISALLOW_COPY_AND_ASSIGN(ManagedState);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_MANAGED_STATE_H_
