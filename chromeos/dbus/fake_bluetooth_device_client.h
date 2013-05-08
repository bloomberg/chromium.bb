// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_BLUETOOTH_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_BLUETOOTH_DEVICE_CLIENT_H_

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client_implementation_type.h"
#include "chromeos/dbus/experimental_bluetooth_agent_service_provider.h"
#include "chromeos/dbus/experimental_bluetooth_device_client.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace chromeos {

// FakeBluetoothDeviceClient simulates the behavior of the Bluetooth Daemon
// device objects and is used both in test cases in place of a mock and on
// the Linux desktop.
class CHROMEOS_EXPORT FakeBluetoothDeviceClient
    : public ExperimentalBluetoothDeviceClient {
 public:
  struct Properties : public ExperimentalBluetoothDeviceClient::Properties {
    explicit Properties(const PropertyChangedCallback & callback);
    virtual ~Properties();

    // dbus::PropertySet override
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeBluetoothDeviceClient();
  virtual ~FakeBluetoothDeviceClient();

  // ExperimentalBluetoothDeviceClient override
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual std::vector<dbus::ObjectPath> GetDevicesForAdapter(
      const dbus::ObjectPath& adapter_path) OVERRIDE;
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE;
  virtual void Connect(const dbus::ObjectPath& object_path,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE;
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          const base::Closure& callback,
                          const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConnectProfile(const dbus::ObjectPath& object_path,
                              const std::string& uuid,
                              const base::Closure& callback,
                              const ErrorCallback& error_callback) OVERRIDE;
  virtual void DisconnectProfile(const dbus::ObjectPath& object_path,
                                 const std::string& uuid,
                                 const base::Closure& callback,
                                 const ErrorCallback& error_callback) OVERRIDE;
  virtual void Pair(const dbus::ObjectPath& object_path,
                    const base::Closure& callback,
                    const ErrorCallback& error_callback) OVERRIDE;
  virtual void CancelPairing(const dbus::ObjectPath& object_path,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) OVERRIDE;

  void SetSimulationIntervalMs(int interval_ms);

  // Simulate discovery of devices for the given adapter.
  void BeginDiscoverySimulation(const dbus::ObjectPath& adapter_path);
  void EndDiscoverySimulation(const dbus::ObjectPath& adapter_path);

  // Remove a device from the set we return for the given adapter.
  void RemoveDevice(const dbus::ObjectPath& adapter_path,
                    const dbus::ObjectPath& device_path);

  // Object paths, names, addresses and bluetooth classes of the devices
  // we can emulate.
  static const char kPairedDevicePath[];
  static const char kPairedDeviceName[];
  static const char kPairedDeviceAddress[];
  static const uint32 kPairedDeviceClass;

  static const char kAppleMousePath[];
  static const char kAppleMouseName[];
  static const char kAppleMouseAddress[];
  static const uint32 kAppleMouseClass;

  static const char kAppleKeyboardPath[];
  static const char kAppleKeyboardName[];
  static const char kAppleKeyboardAddress[];
  static const uint32 kAppleKeyboardClass;

  static const char kVanishingDevicePath[];
  static const char kVanishingDeviceName[];
  static const char kVanishingDeviceAddress[];
  static const uint32 kVanishingDeviceClass;

  static const char kMicrosoftMousePath[];
  static const char kMicrosoftMouseName[];
  static const char kMicrosoftMouseAddress[];
  static const uint32 kMicrosoftMouseClass;

  static const char kMotorolaKeyboardPath[];
  static const char kMotorolaKeyboardName[];
  static const char kMotorolaKeyboardAddress[];
  static const uint32 kMotorolaKeyboardClass;

  static const char kSonyHeadphonesPath[];
  static const char kSonyHeadphonesName[];
  static const char kSonyHeadphonesAddress[];
  static const uint32 kSonyHeadphonesClass;

  static const char kPhonePath[];
  static const char kPhoneName[];
  static const char kPhoneAddress[];
  static const uint32 kPhoneClass;

  static const char kWeirdDevicePath[];
  static const char kWeirdDeviceName[];
  static const char kWeirdDeviceAddress[];
  static const uint32 kWeirdDeviceClass;

  static const char kUnconnectableDevicePath[];
  static const char kUnconnectableDeviceName[];
  static const char kUnconnectableDeviceAddress[];
  static const uint32 kUnconnectableDeviceClass;

  static const char kUnpairableDevicePath[];
  static const char kUnpairableDeviceName[];
  static const char kUnpairableDeviceAddress[];
  static const uint32 kUnpairableDeviceClass;

 private:
  // Property callback passed when we create Properties* structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  void DiscoverySimulationTimer();

  void CompleteSimulatedPairing(
      const dbus::ObjectPath& object_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback);
  void TimeoutSimulatedPairing(
      const dbus::ObjectPath& object_path,
      const ErrorCallback& error_callback);
  void CancelSimulatedPairing(
      const dbus::ObjectPath& object_path,
      const ErrorCallback& error_callback);
  void RejectSimulatedPairing(
      const dbus::ObjectPath& object_path,
      const ErrorCallback& error_callback);
  void FailSimulatedPairing(
      const dbus::ObjectPath& object_path,
      const ErrorCallback& error_callback);
  void AddInputDeviceIfNeeded(
      const dbus::ObjectPath& object_path,
      Properties* properties);

  void PinCodeCallback(
      const dbus::ObjectPath& object_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback,
      ExperimentalBluetoothAgentServiceProvider::Delegate::Status status,
      const std::string& pincode);
  void PasskeyCallback(
      const dbus::ObjectPath& object_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback,
      ExperimentalBluetoothAgentServiceProvider::Delegate::Status status,
      uint32 passkey);
  void ConfirmationCallback(
      const dbus::ObjectPath& object_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback,
      ExperimentalBluetoothAgentServiceProvider::Delegate::Status status);
  void SimulateKeypress(
      uint16 entered,
      const dbus::ObjectPath& object_path,
      const base::Closure& callback,
      const ErrorCallback& error_callback);

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Static properties we return.
  typedef std::map<const dbus::ObjectPath, Properties *> PropertiesMap;
  PropertiesMap properties_map_;
  std::vector<dbus::ObjectPath> device_list_;

  int simulation_interval_ms_;
  uint32_t discovery_simulation_step_;
  bool pairing_cancelled_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_BLUETOOTH_DEVICE_CLIENT_H_
