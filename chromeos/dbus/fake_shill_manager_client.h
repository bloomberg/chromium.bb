// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SHILL_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SHILL_MANAGER_CLIENT_H_

#include <map>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/shill_manager_client.h"

namespace chromeos {

// A fake implementation of ShillManagerClient. This works in close coordination
// with FakeShillServiceClient. FakeShillDeviceClient, and
// FakeShillProfileClient, and is not intended to be used independently.
class CHROMEOS_EXPORT FakeShillManagerClient
    : public ShillManagerClient,
      public ShillManagerClient::TestInterface {
 public:
  FakeShillManagerClient();
  ~FakeShillManagerClient() override;

  // ShillManagerClient overrides
  void Init(dbus::Bus* bus) override;
  void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) override;
  void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) override;
  void GetProperties(const DictionaryValueCallback& callback) override;
  void GetNetworksForGeolocation(
      const DictionaryValueCallback& callback) override;
  void SetProperty(const std::string& name,
                   const base::Value& value,
                   const base::Closure& callback,
                   const ErrorCallback& error_callback) override;
  void RequestScan(const std::string& type,
                   const base::Closure& callback,
                   const ErrorCallback& error_callback) override;
  void EnableTechnology(const std::string& type,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) override;
  void DisableTechnology(const std::string& type,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) override;
  void ConfigureService(const base::DictionaryValue& properties,
                        const ObjectPathCallback& callback,
                        const ErrorCallback& error_callback) override;
  void ConfigureServiceForProfile(const dbus::ObjectPath& profile_path,
                                  const base::DictionaryValue& properties,
                                  const ObjectPathCallback& callback,
                                  const ErrorCallback& error_callback) override;
  void GetService(const base::DictionaryValue& properties,
                  const ObjectPathCallback& callback,
                  const ErrorCallback& error_callback) override;
  void VerifyDestination(const VerificationProperties& properties,
                         const BooleanCallback& callback,
                         const ErrorCallback& error_callback) override;
  void VerifyAndEncryptCredentials(
      const VerificationProperties& properties,
      const std::string& service_path,
      const StringCallback& callback,
      const ErrorCallback& error_callback) override;
  void VerifyAndEncryptData(const VerificationProperties& properties,
                            const std::string& data,
                            const StringCallback& callback,
                            const ErrorCallback& error_callback) override;
  void ConnectToBestServices(const base::Closure& callback,
                             const ErrorCallback& error_callback) override;
  void SetNetworkThrottlingStatus(const NetworkThrottlingStatus& status,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) override;

  ShillManagerClient::TestInterface* GetTestInterface() override;

  // ShillManagerClient::TestInterface overrides.
  void AddDevice(const std::string& device_path) override;
  void RemoveDevice(const std::string& device_path) override;
  void ClearDevices() override;
  void AddTechnology(const std::string& type, bool enabled) override;
  void RemoveTechnology(const std::string& type) override;
  void SetTechnologyInitializing(const std::string& type,
                                 bool initializing) override;
  void AddGeoNetwork(const std::string& technology,
                     const base::DictionaryValue& network) override;
  void AddProfile(const std::string& profile_path) override;
  void ClearProperties() override;
  void SetManagerProperty(const std::string& key,
                          const base::Value& value) override;
  void AddManagerService(const std::string& service_path,
                         bool notify_observers) override;
  void RemoveManagerService(const std::string& service_path) override;
  void ClearManagerServices() override;
  void ServiceStateChanged(const std::string& service_path,
                           const std::string& state) override;
  void SortManagerServices(bool notify) override;
  void SetupDefaultEnvironment() override;
  int GetInteractiveDelay() const override;
  void SetBestServiceToConnect(const std::string& service_path) override;
  const NetworkThrottlingStatus& GetNetworkThrottlingStatus() override;

  // Constants used for testing.
  static const char kFakeEthernetNetworkGuid[];

 private:
  void SetDefaultProperties();
  void PassStubProperties(const DictionaryValueCallback& callback) const;
  void PassStubGeoNetworks(const DictionaryValueCallback& callback) const;
  void CallNotifyObserversPropertyChanged(const std::string& property);
  void NotifyObserversPropertyChanged(const std::string& property);
  base::ListValue* GetListProperty(const std::string& property);
  bool TechnologyEnabled(const std::string& type) const;
  void SetTechnologyEnabled(const std::string& type,
                            const base::Closure& callback,
                            bool enabled);
  std::unique_ptr<base::ListValue> GetEnabledServiceList(
      const std::string& property) const;
  void ScanCompleted(const std::string& device_path,
                     const base::Closure& callback);

  // Parses the command line for Shill stub switches and sets initial states.
  // Uses comma-separated name-value pairs (see SplitStringIntoKeyValuePairs):
  // interactive={delay} - sets delay in seconds for interactive UI
  // {wifi,cellular,etc}={on,off,disabled,none} - sets initial state for type
  void ParseCommandLineSwitch();
  bool ParseOption(const std::string& arg0, const std::string& arg1);
  bool SetInitialNetworkState(std::string type_arg,
                              const std::string& state_arg);
  std::string GetInitialStateForType(const std::string& type,
                                     bool* enabled);

  // Dictionary of property name -> property value
  base::DictionaryValue stub_properties_;

  // Dictionary of technology -> list of property dictionaries
  base::DictionaryValue stub_geo_networks_;

  // Seconds to delay interactive actions
  int interactive_delay_;

  // Initial state for fake services.
  std::map<std::string, std::string> shill_initial_state_map_;

  // Carrier for fake cellular service.
  std::string cellular_carrier_;

  // URL used for cellular activation.
  std::string cellular_olp_;

  // Technology type for fake cellular service.
  std::string cellular_technology_;

  // Roaming state for fake cellular service.
  std::string roaming_state_;

  // Current network throttling status.
  NetworkThrottlingStatus network_throttling_status_ = {false, 0, 0};

  typedef std::map<std::string, base::Value*> ShillPropertyMap;
  typedef std::map<std::string, ShillPropertyMap> DevicePropertyMap;
  DevicePropertyMap shill_device_property_map_;

  base::ObserverList<ShillPropertyChangedObserver>::Unchecked observer_list_;

  // Track the default service for signaling Manager.DefaultService.
  std::string default_service_;

  // 'Best' service to connect to on ConnectToBestServices() calls.
  std::string best_service_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillManagerClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeShillManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SHILL_MANAGER_CLIENT_H_
