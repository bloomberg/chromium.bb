// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CROS_NETWORK_FUNCTIONS_H_
#define CHROMEOS_NETWORK_CROS_NETWORK_FUNCTIONS_H_

// This header is introduced to make it easy to switch from chromeos_network.cc
// to Chrome's own DBus code.  crosbug.com/16557
//
// All calls to functions in chromeos_network.h should be made through
// functions provided by this header.
//
// DO NOT INCLUDE THIS HEADER DIRECTLY, THESE FUNCTIONS ARE BEING DEPRECATED.
// Contact stevenjb@chromium.org and/or see crbug.com/154852

#include <vector>

#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/network/network_ip_config.h"
#include "chromeos/network/network_util.h"

namespace base {

class DictionaryValue;
class Value;

}  // namespace base

namespace chromeos {

// Callback to be called when receiving a SMS.
typedef base::Callback<void(const std::string& modem_device_path,
                            const SMS& message)> MonitorSMSCallback;

// Callback for asynchronous getters.
typedef base::Callback<void(
    const std::string& path,
    const base::DictionaryValue* properties)> NetworkPropertiesCallback;

// Callback for network properties watchers.
typedef base::Callback<void(
    const std::string& path,
    const std::string& key,
    const base::Value& value)> NetworkPropertiesWatcherCallback;

// Base class of signal watchers.
class CHROMEOS_EXPORT CrosNetworkWatcher {
 public:
  virtual ~CrosNetworkWatcher() {}

 protected:
  CrosNetworkWatcher() {}
};

// Activates the cellular modem specified by |service_path| with carrier
// specified by |carrier|.
// |carrier| is NULL or an empty string, this will activate with the currently
// active carrier.
// Returns false on failure and true on success.
CHROMEOS_EXPORT bool CrosActivateCellularModem(const std::string& service_path,
                                               const std::string& carrier);


// Sets a property of a service to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
CHROMEOS_EXPORT void CrosSetNetworkServiceProperty(
    const std::string& service_path,
    const std::string& property,
    const base::Value& value);

// Clears a property of a service.
CHROMEOS_EXPORT void CrosClearNetworkServiceProperty(
    const std::string& service_path,
    const std::string& property);

// Sets a property of a device to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
CHROMEOS_EXPORT void CrosSetNetworkDeviceProperty(
    const std::string& device_path,
    const std::string& property,
    const base::Value& value);

// Sets a property of an ip config to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
CHROMEOS_EXPORT void CrosSetNetworkIPConfigProperty(
    const std::string& ipconfig_path,
    const std::string& property,
    const base::Value& value);

// Sets a property of a manager to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
CHROMEOS_EXPORT void CrosSetNetworkManagerProperty(const std::string& property,
                                                   const base::Value& value);

// Deletes a remembered service from a profile.
CHROMEOS_EXPORT void CrosDeleteServiceFromProfile(
    const std::string& profile_path,
    const std::string& service_path);

// Sets up monitoring of the PropertyChanged signal on the shill manager.
// The provided |callback| will be called whenever a manager property changes.
CHROMEOS_EXPORT CrosNetworkWatcher* CrosMonitorNetworkManagerProperties(
    const NetworkPropertiesWatcherCallback& callback);

// Similar to MonitorNetworkManagerProperties for a specified network service.
CHROMEOS_EXPORT CrosNetworkWatcher* CrosMonitorNetworkServiceProperties(
    const NetworkPropertiesWatcherCallback& callback,
    const std::string& service_path);

// Similar to MonitorNetworkManagerProperties for a specified network device.
CHROMEOS_EXPORT CrosNetworkWatcher* CrosMonitorNetworkDeviceProperties(
    const NetworkPropertiesWatcherCallback& callback,
    const std::string& device_path);

// Similar to MonitorNetworkManagerProperties for a specified network device.
CHROMEOS_EXPORT CrosNetworkWatcher* CrosMonitorSMS(
    const std::string& modem_device_path,
    MonitorSMSCallback callback);

// Connects to the service with the |service_path|.
// Service parameters such as authentication must already be configured.
// Note, a successful invocation of the callback only indicates that
// the connection process has started. You will have to query the
// connection state to determine if the connection was established
// successfully.
CHROMEOS_EXPORT void CrosRequestNetworkServiceConnect(
    const std::string& service_path,
    const NetworkOperationCallback& callback);

// Retrieves the latest info for the manager.
CHROMEOS_EXPORT void CrosRequestNetworkManagerProperties(
    const NetworkPropertiesCallback& callback);

// Retrieves the latest info for a service.
CHROMEOS_EXPORT void CrosRequestNetworkServiceProperties(
    const std::string& service_path,
    const NetworkPropertiesCallback& callback);

// Retrieves the latest info for a particular device.
CHROMEOS_EXPORT void CrosRequestNetworkDeviceProperties(
    const std::string& device_path,
    const NetworkPropertiesCallback& callback);

// Retrieves the list of remembered services for a profile.
CHROMEOS_EXPORT void CrosRequestNetworkProfileProperties(
    const std::string& profile_path,
    const NetworkPropertiesCallback& callback);

// Retrieves the latest info for a profile service entry.
CHROMEOS_EXPORT void CrosRequestNetworkProfileEntryProperties(
    const std::string& profile_path,
    const std::string& profile_entry_path,
    const NetworkPropertiesCallback& callback);

// Requests a wifi service not in the network list (i.e. hidden).
CHROMEOS_EXPORT void CrosRequestHiddenWifiNetworkProperties(
    const std::string& ssid,
    const std::string& security,
    const NetworkPropertiesCallback& callback);

// Requests a new VPN service.
CHROMEOS_EXPORT void CrosRequestVirtualNetworkProperties(
    const std::string& service_name,
    const std::string& server_hostname,
    const std::string& provider_type,
    const NetworkPropertiesCallback& callback);

// Disconnects from network service asynchronously.
CHROMEOS_EXPORT void CrosRequestNetworkServiceDisconnect(
    const std::string& service_path);

// Removes an exisiting network service (e.g. after forgetting a VPN).
CHROMEOS_EXPORT void CrosRequestRemoveNetworkService(
    const std::string& service_path);

// Requests a scan of services of |type|.
// |type| should be is a string recognized by shill's Manager API.
CHROMEOS_EXPORT void CrosRequestNetworkScan(const std::string& network_type);

// Requests enabling or disabling a device.
CHROMEOS_EXPORT void CrosRequestNetworkDeviceEnable(
    const std::string& network_type,
    bool enable);

// Enables or disables PIN protection for a SIM card.
CHROMEOS_EXPORT void CrosRequestRequirePin(
    const std::string& device_path,
    const std::string& pin,
    bool enable,
    const NetworkOperationCallback& callback);

// Enters a PIN to unlock a SIM card.
CHROMEOS_EXPORT void CrosRequestEnterPin(
    const std::string& device_path,
    const std::string& pin,
    const NetworkOperationCallback& callback);

// Enters a PUK to unlock a SIM card whose PIN has been entered
// incorrectly too many times. A new |pin| must be supplied
// along with the |unblock_code| (PUK).
CHROMEOS_EXPORT void CrosRequestUnblockPin(
    const std::string& device_path,
    const std::string& unblock_code,
    const std::string& pin,
    const NetworkOperationCallback& callback);

// Changes the PIN used to unlock a SIM card.
CHROMEOS_EXPORT void CrosRequestChangePin(
    const std::string& device_path,
    const std::string& old_pin,
    const std::string& new_pin,
    const NetworkOperationCallback& callback);

// Proposes to trigger a scan transaction. For cellular networks scan result
// is set in the property Cellular.FoundNetworks.
CHROMEOS_EXPORT void CrosProposeScan(const std::string& device_path);

// Initiates registration on the network specified by network_id, which is in
// the form MCCMNC. If the network ID is the empty string, then switch back to
// automatic registration mode before initiating registration.
CHROMEOS_EXPORT void CrosRequestCellularRegister(
    const std::string& device_path,
    const std::string& network_id,
    const NetworkOperationCallback& callback);

// Enables or disables the specific network device for connection.
// Set offline mode. This will turn off all radios.
// Returns false on failure and true on success.
CHROMEOS_EXPORT bool CrosSetOfflineMode(bool offline);

// Gets a list of all the NetworkIPConfigs using a given device path,
// and returns the information via callback.
CHROMEOS_EXPORT void CrosListIPConfigs(
    const std::string& device_path,
    const NetworkGetIPConfigsCallback& callback);

// DEPRECATED, DO NOT USE: Use the asynchronous CrosListIPConfigs, above,
// instead.
// Gets a list of all the NetworkIPConfigs using a given device path.
// Optionally, you can get ipconfig-paths and the hardware address. Pass NULL as
// |ipconfig_paths| and |hardware_address| if you are not interested in these
// values.
CHROMEOS_EXPORT bool CrosListIPConfigsAndBlock(
    const std::string& device_path,
    NetworkIPConfigVector* ipconfig_vector,
    std::vector<std::string>* ipconfig_paths,
    std::string* hardware_address);

// Refreshes the IP config |ipconfig_path| to pick up changes in
// configuration, and renew the DHCP lease, if any.
CHROMEOS_EXPORT void CrosRequestIPConfigRefresh(
    const std::string& ipconfig_path);

// Configures the network service specified by |properties|.
CHROMEOS_EXPORT void CrosConfigureService(
    const base::DictionaryValue& properties);

// Changes the active cellular carrier.
CHROMEOS_EXPORT void CrosSetCarrier(const std::string& device_path,
                                    const std::string& carrier,
                                    const NetworkOperationCallback& callback);

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CROS_NETWORK_FUNCTIONS_H_
