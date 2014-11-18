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
  virtual ~FakeShillDeviceClient();

  // ShillDeviceClient overrides
  virtual void Init(dbus::Bus* bus) override;
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override;
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override;
  virtual void GetProperties(const dbus::ObjectPath& device_path,
                             const DictionaryValueCallback& callback) override;
  virtual void ProposeScan(const dbus::ObjectPath& device_path,
                           const VoidDBusMethodCallback& callback) override;
  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) override;
  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) override;
  virtual void AddIPConfig(
      const dbus::ObjectPath& device_path,
      const std::string& method,
      const ObjectPathDBusMethodCallback& callback) override;
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) override;
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) override;
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) override;
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) override;
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) override;
  virtual void SetCarrier(const dbus::ObjectPath& device_path,
                          const std::string& carrier,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) override;
  virtual void Reset(const dbus::ObjectPath& device_path,
                     const base::Closure& callback,
                     const ErrorCallback& error_callback) override;
  virtual void PerformTDLSOperation(
      const dbus::ObjectPath& device_path,
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

  virtual ShillDeviceClient::TestInterface* GetTestInterface() override;

  // ShillDeviceClient::TestInterface overrides.
  virtual void AddDevice(const std::string& device_path,
                         const std::string& type,
                         const std::string& name) override;
  virtual void RemoveDevice(const std::string& device_path) override;
  virtual void ClearDevices() override;
  virtual void SetDeviceProperty(const std::string& device_path,
                                 const std::string& name,
                                 const base::Value& value) override;
  virtual std::string GetDevicePathForType(const std::string& type) override;

  void set_tdls_busy_count(int count) { tdls_busy_count_ = count; }

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

  int tdls_busy_count_;  // Number of times to return InProgress for TDLS.

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
