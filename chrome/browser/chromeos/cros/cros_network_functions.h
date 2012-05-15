// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_NETWORK_FUNCTIONS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_NETWORK_FUNCTIONS_H_

// This header is introduced to make it easy to switch from chromeos_network.cc
// to Chrome's own DBus code.  crosbug.com/16557
// All calls to functions in chromeos_network.h should be made through
// functions provided by this header.

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/cros/cellular_data_plan.h"
#include "chrome/browser/chromeos/cros/network_ip_config.h"
#include "third_party/cros/chromeos_network.h"

namespace base {

class DictionaryValue;
class Value;

}  // namespace base

namespace chromeos {

// Callback for asynchronous getters.
typedef base::Callback<void(
    const std::string& path,
    const base::DictionaryValue* properties)> NetworkPropertiesCallback;

// Callback for network properties watchers.
typedef base::Callback<void(
    const std::string& path,
    const std::string& key,
    const base::Value& value)> NetworkPropertiesWatcherCallback;

// Callback for data plan update watchers.
typedef base::Callback<void(
    const std::string& modem_service_path,
    CellularDataPlanVector* data_plan_vector)> DataPlanUpdateWatcherCallback;

// Base class of signal watchers.
class CrosNetworkWatcher {
 public:
  virtual ~CrosNetworkWatcher() {}

 protected:
  CrosNetworkWatcher() {}
};

struct WifiAccessPoint {
  WifiAccessPoint();

  std::string mac_address;  // The mac address of the WiFi node.
  std::string name;         // The SSID of the WiFi node.
  base::Time timestamp;     // Timestamp when this AP was detected.
  int signal_strength;      // Radio signal strength measured in dBm.
  int signal_to_noise;      // Current signal to noise ratio measured in dB.
  int channel;              // Wifi channel number.
};

typedef std::vector<WifiAccessPoint> WifiAccessPointVector;

// Enables/Disables Libcros network functions.
void SetLibcrosNetworkFunctionsEnabled(bool enabled);

// Activates the cellular modem specified by |service_path| with carrier
// specified by |carrier|.
// |carrier| is NULL or an empty string, this will activate with the currently
// active carrier.
// Returns false on failure and true on success.
bool CrosActivateCellularModem(const std::string& service_path,
                               const std::string& carrier);


// Sets a property of a service to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
void CrosSetNetworkServiceProperty(const std::string& service_path,
                                   const std::string& property,
                                   const base::Value& value);

// Clears a property of a service.
void CrosClearNetworkServiceProperty(const std::string& service_path,
                                     const std::string& property);

// Sets a property of a device to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
void CrosSetNetworkDeviceProperty(const std::string& device_path,
                                  const std::string& property,
                                  const base::Value& value);

// Sets a property of an ip config to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
void CrosSetNetworkIPConfigProperty(const std::string& ipconfig_path,
                                    const std::string& property,
                                    const base::Value& value);

// Sets a property of a manager to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
void CrosSetNetworkManagerProperty(const std::string& property,
                                   const base::Value& value);

// Deletes a remembered service from a profile.
void CrosDeleteServiceFromProfile(const std::string& profile_path,
                                  const std::string& service_path);

// Requests an update of the data plans. A callback will be received by any
// object that invoked MonitorCellularDataPlan when up to date data is ready.
void CrosRequestCellularDataPlanUpdate(const std::string& modem_service_path);

// Sets up monitoring of the PropertyChanged signal on the flimflam manager.
// The provided |callback| will be called whenever a manager property changes.
CrosNetworkWatcher* CrosMonitorNetworkManagerProperties(
    const NetworkPropertiesWatcherCallback& callback);

// Similar to MonitorNetworkManagerProperties for a specified network service.
CrosNetworkWatcher* CrosMonitorNetworkServiceProperties(
    const NetworkPropertiesWatcherCallback& callback,
    const std::string& service_path);

// Similar to MonitorNetworkManagerProperties for a specified network device.
CrosNetworkWatcher* CrosMonitorNetworkDeviceProperties(
    const NetworkPropertiesWatcherCallback& callback,
    const std::string& device_path);

// Sets up monitoring of the cellular data plan updates from Cashew.
CrosNetworkWatcher* CrosMonitorCellularDataPlan(
    const DataPlanUpdateWatcherCallback& callback);

// Similar to MonitorNetworkManagerProperties for a specified network device.
CrosNetworkWatcher* CrosMonitorSMS(const std::string& modem_device_path,
                                   MonitorSMSCallback callback,
                                   void* object);

// Connects to the service with the |service_path|.
// Service parameters such as authentication must already be configured.
// Note, a successful invocation of the callback only indicates that
// the connection process has started. You will have to query the
// connection state to determine if the connection was established
// successfully.
void CrosRequestNetworkServiceConnect(const std::string& service_path,
                                      NetworkActionCallback callback,
                                      void* object);

// Retrieves the latest info for the manager.
void CrosRequestNetworkManagerProperties(
    const NetworkPropertiesCallback& callback);

// Retrieves the latest info for a service.
void CrosRequestNetworkServiceProperties(
    const std::string& service_path,
    const NetworkPropertiesCallback& callback);

// Retrieves the latest info for a particular device.
void CrosRequestNetworkDeviceProperties(
    const std::string& device_path,
    const NetworkPropertiesCallback& callback);

// Retrieves the list of remembered services for a profile.
void CrosRequestNetworkProfileProperties(
    const std::string& profile_path,
    const NetworkPropertiesCallback& callback);

// Retrieves the latest info for a profile service entry.
void CrosRequestNetworkProfileEntryProperties(
    const std::string& profile_path,
    const std::string& profile_entry_path,
    const NetworkPropertiesCallback& callback);

// Requests a wifi service not in the network list (i.e. hidden).
void CrosRequestHiddenWifiNetworkProperties(
    const std::string& ssid,
    const std::string& security,
    const NetworkPropertiesCallback& callback);

// Requests a new VPN service.
void CrosRequestVirtualNetworkProperties(
    const std::string& service_name,
    const std::string& server_hostname,
    const std::string& provider_type,
    const NetworkPropertiesCallback& callback);

// Disconnects from network service asynchronously.
void CrosRequestNetworkServiceDisconnect(const std::string& service_path);

// Removes an exisiting network service (e.g. after forgetting a VPN).
void CrosRequestRemoveNetworkService(const std::string& service_path);

// Requests a scan of services of |type|.
// |type| should be is a string recognized by flimflam's Manager API.
void CrosRequestNetworkScan(const std::string& network_type);

// Requests enabling or disabling a device.
void CrosRequestNetworkDeviceEnable(const std::string& network_type,
                                    bool enable);

// Enables or disables PIN protection for a SIM card.
void CrosRequestRequirePin(const std::string& device_path,
                           const std::string& pin,
                           bool enable,
                           NetworkActionCallback callback,
                           void* object);

// Enters a PIN to unlock a SIM card.
void CrosRequestEnterPin(const std::string& device_path,
                         const std::string& pin,
                         NetworkActionCallback callback,
                         void* object);

// Enters a PUK to unlock a SIM card whose PIN has been entered
// incorrectly too many times. A new |pin| must be supplied
// along with the |unblock_code| (PUK).
void CrosRequestUnblockPin(const std::string& device_path,
                           const std::string& unblock_code,
                           const std::string& pin,
                           NetworkActionCallback callback,
                           void* object);

// Changes the PIN used to unlock a SIM card.
void CrosRequestChangePin(const std::string& device_path,
                          const std::string& old_pin,
                          const std::string& new_pin,
                          NetworkActionCallback callback,
                          void* object);

// Proposes to trigger a scan transaction. For cellular networks scan result
// is set in the property Cellular.FoundNetworks.
void CrosProposeScan(const std::string& device_path);

// Initiates registration on the network specified by network_id, which is in
// the form MCCMNC. If the network ID is the empty string, then switch back to
// automatic registration mode before initiating registration.
void CrosRequestCellularRegister(const std::string& device_path,
                                 const std::string& network_id,
                                 NetworkActionCallback callback,
                                 void* object);

// Enables or disables the specific network device for connection.
// Set offline mode. This will turn off all radios.
// Returns false on failure and true on success.
bool CrosSetOfflineMode(bool offline);

// Gets a list of all the NetworkIPConfigs using a given device path.
// Optionally, you can get ipconfig-paths and the hardware address.
// Pass NULL as |ipconfig_paths| and |hardware_address| if you are not
// interested in these values.
bool CrosListIPConfigs(const std::string& device_path,
                       NetworkIPConfigVector* ipconfig_vector,
                       std::vector<std::string>* ipconfig_paths,
                       std::string* hardware_address);

// Adds a IPConfig of the given type to the device
bool CrosAddIPConfig(const std::string& device_path, IPConfigType type);

// Removes an existing IP Config
bool CrosRemoveIPConfig(const std::string& ipconfig_path);

// Reads out the results of the last wifi scan. These results are not
// pre-cached in the library, so the call may block whilst the results are
// read over IPC.
// Returns false if an error occurred in reading the results. Note that
// a true return code only indicates the result set was successfully read,
// it does not imply a scan has successfully completed yet.
bool CrosGetWifiAccessPoints(WifiAccessPointVector* result);

// Configures the network service specified by |properties|.
void CrosConfigureService(const base::DictionaryValue& properties);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CROS_NETWORK_FUNCTIONS_H_
