// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_CROS_NETWORK_FUNCTIONS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_CROS_NETWORK_FUNCTIONS_H_

// This header is introduced to make it easy to switch from chromeos_network.cc
// to Chrome's own DBus code.  crosbug.com/16557
// All calls to functions in chromeos_network.h should be made through
// functions provided by this header.

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/cros/chromeos_network.h"

namespace base {

class DictionaryValue;
class Value;

}  // namespace base

namespace chromeos {

// Callback for asynchronous getters.
typedef base::Callback<void(
    const char* path,
    const base::DictionaryValue* properties)> NetworkPropertiesCallback;

// Enables/Disables Libcros network functions.
void SetLibcrosNetworkFunctionsEnabled(bool enabled);

// Activates the cellular modem specified by |service_path| with carrier
// specified by |carrier|.
// |carrier| is NULL or an empty string, this will activate with the currently
// active carrier.
// Returns false on failure and true on success.
bool CrosActivateCellularModem(const char* service_path, const char* carrier);


// Sets a property of a service to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
void CrosSetNetworkServiceProperty(const char* service_path,
                                   const char* property,
                                   const base::Value& value);

// Clears a property of a service.
void CrosClearNetworkServiceProperty(const char* service_path,
                                     const char* property);

// Sets a property of a device to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
void CrosSetNetworkDeviceProperty(const char* device_path,
                                  const char* property,
                                  const base::Value& value);

// Sets a property of an ip config to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
void CrosSetNetworkIPConfigProperty(const char* ipconfig_path,
                                    const char* property,
                                    const base::Value& value);

// Sets a property of a manager to the provided value.
// Success is indicated by the receipt of a matching PropertyChanged signal.
void CrosSetNetworkManagerProperty(const char* property,
                                   const base::Value& value);

// Deletes a remembered service from a profile.
void CrosDeleteServiceFromProfile(const char* profile_path,
                                  const char* service_path);

// Requests an update of the data plans. A callback will be received by any
// object that invoked MonitorCellularDataPlan when up to date data is ready.
void CrosRequestCellularDataPlanUpdate(const char* modem_service_path);

// Sets up monitoring of the PropertyChanged signal on the flimflam manager.
// The provided |callback| will be called whenever a manager property changes.
// |object| will be supplied as the first argument to the callback.
NetworkPropertiesMonitor CrosMonitorNetworkManagerProperties(
    MonitorPropertyGValueCallback callback,
    void* object);

// Similar to MonitorNetworkManagerProperties for a specified network service.
NetworkPropertiesMonitor CrosMonitorNetworkServiceProperties(
    MonitorPropertyGValueCallback callback,
    const char* service_path,
    void* object);

// Similar to MonitorNetworkManagerProperties for a specified network device.
NetworkPropertiesMonitor CrosMonitorNetworkDeviceProperties(
    MonitorPropertyGValueCallback callback,
    const char* device_path,
    void* object);

// Disconnects a PropertyChangeMonitor.
void CrosDisconnectNetworkPropertiesMonitor(
    NetworkPropertiesMonitor monitor);

// Sets up monitoring of the cellular data plan updates from Cashew.
DataPlanUpdateMonitor CrosMonitorCellularDataPlan(
    MonitorDataPlanCallback callback,
    void* object);

// Disconnects a DataPlanUpdateMonitor.
void CrosDisconnectDataPlanUpdateMonitor(DataPlanUpdateMonitor monitor);

// Similar to MonitorNetworkManagerProperties for a specified network device.
SMSMonitor CrosMonitorSMS(const char* modem_device_path,
                          MonitorSMSCallback callback,
                          void* object);

// Disconnects a SMSMonitor.
void CrosDisconnectSMSMonitor(SMSMonitor monitor);

// Connects to the service with the |service_path|.
// Service parameters such as authentication must already be configured.
// Note, a successful invocation of the callback only indicates that
// the connection process has started. You will have to query the
// connection state to determine if the connection was established
// successfully.
void CrosRequestNetworkServiceConnect(const char* service_path,
                                      NetworkActionCallback callback,
                                      void* object);

// Retrieves the latest info for the manager.
void CrosRequestNetworkManagerProperties(
    const NetworkPropertiesCallback& callback);

// Retrieves the latest info for a service.
void CrosRequestNetworkServiceProperties(
    const char* service_path,
    const NetworkPropertiesCallback& callback);

// Retrieves the latest info for a particular device.
void CrosRequestNetworkDeviceProperties(
    const char* device_path,
    const NetworkPropertiesCallback& callback);

// Retrieves the list of remembered services for a profile.
void CrosRequestNetworkProfileProperties(
    const char* profile_path,
    const NetworkPropertiesCallback& callback);

// Retrieves the latest info for a profile service entry.
void CrosRequestNetworkProfileEntryProperties(
    const char* profile_path,
    const char* profile_entry_path,
    const NetworkPropertiesCallback& callback);

// Requests a wifi service not in the network list (i.e. hidden).
void CrosRequestHiddenWifiNetworkProperties(
    const char* ssid,
    const char* security,
    const NetworkPropertiesCallback& callback);

// Requests a new VPN service.
void CrosRequestVirtualNetworkProperties(
    const char* service_name,
    const char* server_hostname,
    const char* provider_type,
    const NetworkPropertiesCallback& callback);

// Disconnects from network service asynchronously.
void CrosRequestNetworkServiceDisconnect(const char* service_path);

// Removes an exisiting network service (e.g. after forgetting a VPN).
void CrosRequestRemoveNetworkService(const char* service_path);

// Requests a scan of services of |type|.
// |type| should be is a string recognized by flimflam's Manager API.
void CrosRequestNetworkScan(const char* network_type);

// Requests enabling or disabling a device.
void CrosRequestNetworkDeviceEnable(const char* network_type, bool enable);

// Enables or disables PIN protection for a SIM card.
void CrosRequestRequirePin(const char* device_path,
                           const char* pin,
                           bool enable,
                           NetworkActionCallback callback,
                           void* object);

// Enters a PIN to unlock a SIM card.
void CrosRequestEnterPin(const char* device_path,
                         const char* pin,
                         NetworkActionCallback callback,
                         void* object);

// Enters a PUK to unlock a SIM card whose PIN has been entered
// incorrectly too many times. A new |pin| must be supplied
// along with the |unblock_code| (PUK).
void CrosRequestUnblockPin(const char* device_path,
                           const char* unblock_code,
                           const char* pin,
                           NetworkActionCallback callback,
                           void* object);

// Changes the PIN used to unlock a SIM card.
void CrosRequestChangePin(const char* device_path,
                          const char* old_pin,
                          const char* new_pin,
                          NetworkActionCallback callback,
                          void* object);

// Proposes to trigger a scan transaction. For cellular networks scan result
// is set in the property Cellular.FoundNetworks.
void CrosProposeScan(const char* device_path);

// Initiates registration on the network specified by network_id, which is in
// the form MCCMNC. If the network ID is the empty string, then switch back to
// automatic registration mode before initiating registration.
void CrosRequestCellularRegister(const char* device_path,
                                 const char* network_id,
                                 NetworkActionCallback callback,
                                 void* object);

// Enables or disables the specific network device for connection.
// Set offline mode. This will turn off all radios.
// Returns false on failure and true on success.
bool CrosSetOfflineMode(bool offline);

// Gets a list of all the IPConfigs using a given device path
IPConfigStatus* CrosListIPConfigs(const char* device_path);

// Adds a IPConfig of the given type to the device
bool CrosAddIPConfig(const char* device_path, IPConfigType type);

// Removes an existing IP Config
bool CrosRemoveIPConfig(IPConfig* config);

// Frees out a full status
void CrosFreeIPConfigStatus(IPConfigStatus* status);

// Retrieves the list of visible network objects. This structure is not cached
// within the DLL, and is fetched via d-bus on each call.
// Ownership of the DeviceNetworkList is returned to the caller; use
// FreeDeviceNetworkList to release it.
DeviceNetworkList* CrosGetDeviceNetworkList();

// Deletes a DeviceNetworkList type that was allocated in the ChromeOS dll. We
// need to do this to safely pass data over the dll boundary between our .so
// and Chrome.
void CrosFreeDeviceNetworkList(DeviceNetworkList* network_list);

// Configures the network service specified by |properties|.
// |identifier| can be the service path, guid, or any other identifier
// specified by the calling code; it is ignored by libcros and flimflam,
// except to pass it back in |callback| as |path|.
void CrosConfigureService(const char* identifier,
                          const base::DictionaryValue& properties,
                          NetworkActionCallback callback,
                          void* object);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_CROS_NETWORK_FUNCTIONS_H_
