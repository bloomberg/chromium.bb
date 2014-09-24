// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SHILL_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SHILL_MANAGER_CLIENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/shill_manager_client.h"

namespace net {
class IPEndPoint;
}

namespace chromeos {

// A fake implementation of ShillManagerClient. This works in close coordination
// with FakeShillServiceClient. FakeShillDeviceClient, and
// FakeShillProfileClient, and is not intended to be used independently.
class CHROMEOS_EXPORT FakeShillManagerClient
    : public ShillManagerClient,
      public ShillManagerClient::TestInterface {
 public:
  FakeShillManagerClient();
  virtual ~FakeShillManagerClient();

  // ShillManagerClient overrides
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void GetProperties(const DictionaryValueCallback& callback) OVERRIDE;
  virtual void GetNetworksForGeolocation(
      const DictionaryValueCallback& callback) OVERRIDE;
  virtual void SetProperty(const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE;
  virtual void RequestScan(const std::string& type,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE;
  virtual void EnableTechnology(
      const std::string& type,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void DisableTechnology(
      const std::string& type,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConfigureService(
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConfigureServiceForProfile(
      const dbus::ObjectPath& profile_path,
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void GetService(
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void VerifyDestination(const VerificationProperties& properties,
                                 const BooleanCallback& callback,
                                 const ErrorCallback& error_callback) OVERRIDE;
  virtual void VerifyAndEncryptCredentials(
      const VerificationProperties& properties,
      const std::string& service_path,
      const StringCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void VerifyAndEncryptData(
      const VerificationProperties& properties,
      const std::string& data,
      const StringCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConnectToBestServices(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void AddWakeOnPacketConnection(
      const net::IPEndPoint& ip_connection,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void RemoveWakeOnPacketConnection(
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void RemoveAllWakeOnPacketConnections(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

  virtual ShillManagerClient::TestInterface* GetTestInterface() OVERRIDE;

  // ShillManagerClient::TestInterface overrides.
  virtual void AddDevice(const std::string& device_path) OVERRIDE;
  virtual void RemoveDevice(const std::string& device_path) OVERRIDE;
  virtual void ClearDevices() OVERRIDE;
  virtual void AddTechnology(const std::string& type, bool enabled) OVERRIDE;
  virtual void RemoveTechnology(const std::string& type) OVERRIDE;
  virtual void SetTechnologyInitializing(const std::string& type,
                                         bool initializing) OVERRIDE;
  virtual void AddGeoNetwork(const std::string& technology,
                             const base::DictionaryValue& network) OVERRIDE;
  virtual void AddProfile(const std::string& profile_path) OVERRIDE;
  virtual void ClearProperties() OVERRIDE;
  virtual void SetManagerProperty(const std::string& key,
                                  const base::Value& value) OVERRIDE;
  virtual void AddManagerService(const std::string& service_path,
                                 bool notify_observers) OVERRIDE;
  virtual void RemoveManagerService(const std::string& service_path) OVERRIDE;
  virtual void ClearManagerServices() OVERRIDE;
  virtual void ServiceStateChanged(const std::string& service_path,
                                   const std::string& state) OVERRIDE;
  virtual void SortManagerServices(bool notify) OVERRIDE;
  virtual void SetupDefaultEnvironment() OVERRIDE;
  virtual int GetInteractiveDelay() const OVERRIDE;
  virtual void SetBestServiceToConnect(
      const std::string& service_path) OVERRIDE;

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
  base::ListValue* GetEnabledServiceList(const std::string& property) const;
  void ScanCompleted(const std::string& device_path,
                     const base::Closure& callback);

  // Parses the command line for Shill stub switches and sets initial states.
  // Uses comma-separated name-value pairs (see SplitStringIntoKeyValuePairs):
  // interactive={delay} - sets delay in seconds for interactive UI
  // {wifi,cellular,etc}={on,off,disabled,none} - sets initial state for type
  void ParseCommandLineSwitch();
  bool ParseOption(const std::string& arg0, const std::string& arg1);
  bool SetInitialNetworkState(std::string type_arg, std::string state_arg);
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
  typedef std::map<std::string, base::Value*> ShillPropertyMap;
  typedef std::map<std::string, ShillPropertyMap> DevicePropertyMap;
  DevicePropertyMap shill_device_property_map_;

  ObserverList<ShillPropertyChangedObserver> observer_list_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillManagerClient> weak_ptr_factory_;

  // Track the default service for signaling Manager.DefaultService.
  std::string default_service_;

  // 'Best' service to connect to on ConnectToBestServices() calls.
  std::string best_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeShillManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SHILL_MANAGER_CLIENT_H_
