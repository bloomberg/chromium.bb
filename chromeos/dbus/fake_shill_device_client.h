// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SHILL_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SHILL_DEVICE_CLIENT_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/shill_device_client.h"

namespace chromeos {

// A fake implementation of ShillDeviceClient.
// Implemented: Stub cellular device for SMS testing.
class CHROMEOS_EXPORT FakeShillDeviceClient
    : public ShillDeviceClient,
      public ShillDeviceClient::TestInterface {
 public:
  FakeShillDeviceClient();
  ~FakeShillDeviceClient() override;

  // ShillDeviceClient overrides
  void Init(dbus::Bus* bus) override;
  void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override;
  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override;
  void GetProperties(const dbus::ObjectPath& device_path,
                     const DictionaryValueCallback& callback) override;
  void ProposeScan(const dbus::ObjectPath& device_path,
                   const VoidDBusMethodCallback& callback) override;
  void SetProperty(const dbus::ObjectPath& device_path,
                   const std::string& name,
                   const base::Value& value,
                   const base::Closure& callback,
                   const ErrorCallback& error_callback) override;
  void ClearProperty(const dbus::ObjectPath& device_path,
                     const std::string& name,
                     const VoidDBusMethodCallback& callback) override;
  void AddIPConfig(const dbus::ObjectPath& device_path,
                   const std::string& method,
                   const ObjectPathDBusMethodCallback& callback) override;
  void RequirePin(const dbus::ObjectPath& device_path,
                  const std::string& pin,
                  bool require,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void EnterPin(const dbus::ObjectPath& device_path,
                const std::string& pin,
                const base::Closure& callback,
                const ErrorCallback& error_callback) override;
  void UnblockPin(const dbus::ObjectPath& device_path,
                  const std::string& puk,
                  const std::string& pin,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void ChangePin(const dbus::ObjectPath& device_path,
                 const std::string& old_pin,
                 const std::string& new_pin,
                 const base::Closure& callback,
                 const ErrorCallback& error_callback) override;
  void Register(const dbus::ObjectPath& device_path,
                const std::string& network_id,
                const base::Closure& callback,
                const ErrorCallback& error_callback) override;
  void SetCarrier(const dbus::ObjectPath& device_path,
                  const std::string& carrier,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void Reset(const dbus::ObjectPath& device_path,
             const base::Closure& callback,
             const ErrorCallback& error_callback) override;
  void PerformTDLSOperation(const dbus::ObjectPath& device_path,
                            const std::string& operation,
                            const std::string& peer,
                            const StringCallback& callback,
                            const ErrorCallback& error_callback) override;
  void AddWakeOnPacketConnection(
      const dbus::ObjectPath& device_path,
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;
  void RemoveWakeOnPacketConnection(
      const dbus::ObjectPath& device_path,
      const net::IPEndPoint& ip_endpoint,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;
  void RemoveAllWakeOnPacketConnections(
      const dbus::ObjectPath& device_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override;

  ShillDeviceClient::TestInterface* GetTestInterface() override;

  // ShillDeviceClient::TestInterface overrides.
  void AddDevice(const std::string& device_path,
                 const std::string& type,
                 const std::string& name) override;
  void RemoveDevice(const std::string& device_path) override;
  void ClearDevices() override;
  void SetDeviceProperty(const std::string& device_path,
                         const std::string& name,
                         const base::Value& value) override;
  std::string GetDevicePathForType(const std::string& type) override;
  void SetTDLSBusyCount(int count) override;
  void SetTDLSState(const std::string& state) override;

 private:
  typedef ObserverList<ShillPropertyChangedObserver> PropertyObserverList;

  void SetDefaultProperties();
  void PassStubDeviceProperties(const dbus::ObjectPath& device_path,
                                const DictionaryValueCallback& callback) const;

  // Posts a task to run a void callback with status code |status|.
  void PostVoidCallback(const VoidDBusMethodCallback& callback,
                        DBusMethodCallStatus status);

  void NotifyObserversPropertyChanged(const dbus::ObjectPath& device_path,
                                      const std::string& property);
  base::DictionaryValue* GetDeviceProperties(const std::string& device_path);
  PropertyObserverList& GetObserverList(const dbus::ObjectPath& device_path);

  // Dictionary of <device_name, Dictionary>.
  base::DictionaryValue stub_devices_;
  // Observer list for each device.
  std::map<dbus::ObjectPath, PropertyObserverList*> observer_list_;

  // Number of times to return InProgress for TDLS. Set to -1 to emulate
  // TDLS failure.
  int initial_tdls_busy_count_;

  // Current TDLS busy count.
  int tdls_busy_count_;

  // Fake state for TDLS.
  std::string tdls_state_;

  // Wake on packet connections for each device.
  std::map<dbus::ObjectPath, std::set<net::IPEndPoint> >
      wake_on_packet_connections_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillDeviceClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeShillDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SHILL_DEVICE_CLIENT_H_
