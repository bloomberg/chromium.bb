// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_CACHE_ENTRY_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_CACHE_ENTRY_H_

#include <memory>
#include <string>

#include "base/logging.h"

namespace chromeos {

namespace tether {

// Entry to place within a HostScanCache. Contains metadata about a Tether host
// which has been scanned.
class HostScanCacheEntry {
 public:
  class Builder {
   public:
    Builder();
    virtual ~Builder();

    std::unique_ptr<HostScanCacheEntry> Build();
    Builder& SetTetherNetworkGuid(const std::string& tether_network_guid);
    Builder& SetDeviceName(const std::string& device_name);
    Builder& SetCarrier(const std::string& carrier);
    Builder& SetBatteryPercentage(int battery_percentage);
    Builder& SetSignalStrength(int signal_strength);
    Builder& SetSetupRequired(bool setup_required);

   private:
    std::string tether_network_guid_;
    std::string device_name_;
    std::string carrier_;
    int battery_percentage_ = 100;
    int signal_strength_ = 100;
    bool setup_required_ = false;
  };

  HostScanCacheEntry(const HostScanCacheEntry& other);
  virtual ~HostScanCacheEntry();

  bool operator==(const HostScanCacheEntry& other) const;

  // GUID corresponding to the scanned device.
  const std::string tether_network_guid;

  // Human-readable name of the device.
  const std::string device_name;

  // Human-readable name of the cellular carrier.
  const std::string carrier;

  // Percentage from 0 to 100 (inclusive).
  const int battery_percentage;

  // Signal strength from 0 to 100 (inclusive). Note that this is different from
  // the "connection strength" value that is sent over the Tether protocol,
  // which ranges from 0 to 4 (see ConnectTetheringResponse).
  const int signal_strength;

  // Whether the scanned host requires first-time setup before use (i.e.,
  // whether the user must interact with UI on the phone to initiate tethering).
  const bool setup_required;

 protected:
  HostScanCacheEntry(const std::string& tether_network_guid,
                     const std::string& device_name,
                     const std::string& carrier,
                     int battery_percentage,
                     int signal_strength,
                     bool setup_required);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_CACHE_ENTRY_H_
