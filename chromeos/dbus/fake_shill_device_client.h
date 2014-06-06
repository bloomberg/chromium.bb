// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SHILL_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SHILL_DEVICE_CLIENT_H_

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
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) OVERRIDE;
  virtual void GetProperties(const dbus::ObjectPath& device_path,
                             const DictionaryValueCallback& callback) OVERRIDE;
  virtual void ProposeScan(const dbus::ObjectPath& device_path,
                           const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void SetProperty(const dbus::ObjectPath& device_path,
                           const std::string& name,
                           const base::Value& value,
                           const base::Closure& callback,
                           const ErrorCallback& error_callback) OVERRIDE;
  virtual void ClearProperty(const dbus::ObjectPath& device_path,
                             const std::string& name,
                             const VoidDBusMethodCallback& callback) OVERRIDE;
  virtual void AddIPConfig(
      const dbus::ObjectPath& device_path,
      const std::string& method,
      const ObjectPathDBusMethodCallback& callback) OVERRIDE;
  virtual void RequirePin(const dbus::ObjectPath& device_path,
                          const std::string& pin,
                          bool require,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void EnterPin(const dbus::ObjectPath& device_path,
                        const std::string& pin,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE;
  virtual void UnblockPin(const dbus::ObjectPath& device_path,
                          const std::string& puk,
                          const std::string& pin,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void ChangePin(const dbus::ObjectPath& device_path,
                         const std::string& old_pin,
                         const std::string& new_pin,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) OVERRIDE;
  virtual void Register(const dbus::ObjectPath& device_path,
                        const std::string& network_id,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) OVERRIDE;
  virtual void SetCarrier(const dbus::ObjectPath& device_path,
                          const std::string& carrier,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void Reset(const dbus::ObjectPath& device_path,
                     const base::Closure& callback,
                     const ErrorCallback& error_callback) OVERRIDE;
  virtual void PerformTDLSOperation(
      const dbus::ObjectPath& device_path,
      const std::string& operation,
      const std::string& peer,
      const StringCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;

  virtual ShillDeviceClient::TestInterface* GetTestInterface() OVERRIDE;

  // ShillDeviceClient::TestInterface overrides.
  virtual void AddDevice(const std::string& device_path,
                         const std::string& type,
                         const std::string& name) OVERRIDE;
  virtual void RemoveDevice(const std::string& device_path) OVERRIDE;
  virtual void ClearDevices() OVERRIDE;
  virtual void SetDeviceProperty(const std::string& device_path,
                                 const std::string& name,
                                 const base::Value& value) OVERRIDE;
  virtual std::string GetDevicePathForType(const std::string& type) OVERRIDE;

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

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillDeviceClient> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeShillDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SHILL_DEVICE_CLIENT_H_
