// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_DEVICE_STATE_H_
#define CHROMEOS_NETWORK_DEVICE_STATE_H_

#include "chromeos/network/managed_state.h"

namespace chromeos {

// Simple class to provide device state information. Similar to NetworkState;
// see network_state.h for usage guidelines.
class CHROMEOS_EXPORT DeviceState : public ManagedState {
 public:
  explicit DeviceState(const std::string& path);
  virtual ~DeviceState();

  // ManagedState overrides
  virtual bool PropertyChanged(const std::string& key,
                               const base::Value& value) OVERRIDE;

  // Accessors
  const std::string& mac_address() const { return mac_address_; }
  bool provider_requires_roaming() const { return provider_requires_roaming_; }
  bool support_network_scan() const { return support_network_scan_; }
  bool scanning() const { return scanning_; }
  std::string sim_lock_type() const { return sim_lock_type_; }

  // Returns true if the technology family is GSM and sim_present_ is false.
  bool IsSimAbsent() const;

 private:
  // Common Device Properties
  std::string mac_address_;
  // Cellular specific propeties
  std::string home_provider_id_;
  bool provider_requires_roaming_;
  bool support_network_scan_;
  bool scanning_;
  std::string technology_family_;
  std::string sim_lock_type_;
  bool sim_present_;

  DISALLOW_COPY_AND_ASSIGN(DeviceState);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_DEVICE_STATE_H_
