// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_
#define CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkStateHandler;

class CHROMEOS_EXPORT NetworkDeviceHandlerImpl
    : public NetworkDeviceHandler,
      public NetworkStateHandlerObserver {
 public:
  virtual ~NetworkDeviceHandlerImpl();

  // NetworkDeviceHandler overrides
  virtual void GetDeviceProperties(
      const std::string& device_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) const OVERRIDE;

  virtual void SetDeviceProperty(
      const std::string& device_path,
      const std::string& property_name,
      const base::Value& value,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) OVERRIDE;

  virtual void RequestRefreshIPConfigs(
      const std::string& device_path,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) OVERRIDE;

  virtual void ProposeScan(const std::string& device_path,
                           const base::Closure& callback,
                           const network_handler::ErrorCallback& error_callback)
      OVERRIDE;

  virtual void RegisterCellularNetwork(
      const std::string& device_path,
      const std::string& network_id,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback) OVERRIDE;

  virtual void SetCarrier(const std::string& device_path,
                          const std::string& carrier,
                          const base::Closure& callback,
                          const network_handler::ErrorCallback& error_callback)
      OVERRIDE;

  virtual void RequirePin(const std::string& device_path,
                          bool require_pin,
                          const std::string& pin,
                          const base::Closure& callback,
                          const network_handler::ErrorCallback& error_callback)
      OVERRIDE;

  virtual void EnterPin(const std::string& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const network_handler::ErrorCallback& error_callback)
      OVERRIDE;

  virtual void UnblockPin(const std::string& device_path,
                          const std::string& puk,
                          const std::string& new_pin,
                          const base::Closure& callback,
                          const network_handler::ErrorCallback& error_callback)
      OVERRIDE;

  virtual void ChangePin(const std::string& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const network_handler::ErrorCallback& error_callback)
      OVERRIDE;

  virtual void SetCellularAllowRoaming(bool allow_roaming) OVERRIDE;

  virtual void SetWifiTDLSEnabled(
      const std::string& ip_or_mac_address,
      bool enabled,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) OVERRIDE;

  virtual void GetWifiTDLSStatus(
      const std::string& ip_or_mac_address,
      const network_handler::StringResultCallback& callback,
      const network_handler::ErrorCallback& error_callback) OVERRIDE;

  // NetworkStateHandlerObserver overrides
  virtual void DeviceListChanged() OVERRIDE;

 private:
  friend class NetworkHandler;
  friend class NetworkDeviceHandlerTest;

  NetworkDeviceHandlerImpl();

  void Init(NetworkStateHandler* network_state_handler);

  // Apply the current value of |cellular_allow_roaming_| to all existing
  // cellular devices of Shill.
  void ApplyCellularAllowRoamingToShill();

  NetworkStateHandler* network_state_handler_;
  bool cellular_allow_roaming_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDeviceHandlerImpl);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_DEVICE_HANDLER_IMPL_H_
