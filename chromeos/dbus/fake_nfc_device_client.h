// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_NFC_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_NFC_DEVICE_CLIENT_H_

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/nfc_client_helpers.h"
#include "chromeos/dbus/nfc_device_client.h"

namespace chromeos {

// FakeNfcDeviceClient simulates the behavior of the NFC device objects
// and is used both in test cases in place of a mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeNfcDeviceClient : public NfcDeviceClient {
 public:
  // The fake device object path.
  static const char kDevicePath[];

  // The default simulation timeout interval.
  static const int kDefaultSimulationTimeoutMilliseconds;

  // Properties structure that provides fake behavior for D-Bus calls.
  struct Properties : public NfcDeviceClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // dbus::PropertySet overrides.
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeNfcDeviceClient();
  virtual ~FakeNfcDeviceClient();

  // NfcDeviceClient overrides.
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual std::vector<dbus::ObjectPath> GetDevicesForAdapter(
      const dbus::ObjectPath& adapter_path) OVERRIDE;
  virtual Properties* GetProperties(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void Push(
      const dbus::ObjectPath& object_path,
      const base::DictionaryValue& attributes,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) OVERRIDE;

  // Simulates the appearance of a device. The fake device will show up after
  // exactly |visibility_delay| milliseconds, and will simulate pushing a single
  // record to the local fake adapter after exactly |record_push_delay|
  // milliseconds after the the device appears. |visibility_delay| must have a
  // non-negative value. |record_push_delay| CAN be negative: if it has a
  // negative value, the record push step will not be simulated. The
  // side-effects of this method occur asynchronously, i.e. even with arguments
  // with value 0, the pairing won't take place until after this method has
  // returned.
  void BeginPairingSimulation(int visibility_delay, int record_push_delay);

  // If device pairing was previously started, simulates the disappearance of
  // the device. Any device objects presented and their records will disappear
  // after this call. Delayed events that were set up by a previous call to
  // BeginPairing() will be canceled through a call to EndPairing().
  void EndPairingSimulation();

  // Enables or disables automatic unpairing. When enabled, a pairing
  // simulation will end |simulation_timeout| milliseconds after records have
  // been exposed (or after the tag has been exposed, if |record_push_delay| was
  // given as a negative value to BeginPairingSimulation) This is enabled by
  // default and the timeout is set to |kDefaultSimulationTimeoutMilliseconds|.
  void EnableSimulationTimeout(int simulation_timeout);
  void DisableSimulationTimeout();

  // Tells the FakeNfcDeviceClient to add the records in |record_paths| to its
  // list of records exposed for |kDevicePath|. This method will immediately
  // assign the records and trigger a property changed signal, only if the
  // device is currently visible.
  void SetRecords(const std::vector<dbus::ObjectPath>& record_paths);

  // Tells the FakeNfcDeviceClient to clear the list of records exposed for
  // |kDevicePath|. This method takes effect immediately and triggers a
  // property changed signal.
  void ClearRecords();

  // Returns true, if a pairing simulation is currently going on.
  bool device_visible() const { return device_visible_; }

 private:
  // Property changed callback passed when we create Properties* structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // Makes the fake device visible (if it is not already so) and simulates a
  // record push after |record_push_delay| seconds. Posted by BeginPairing().
  void MakeDeviceVisible(int record_push_delay);

  // Makes the fake records visible. Called by MakeDeviceVisible().
  void MakeRecordsVisible();

  // Called when the simulation timeout expires.
  void HandleSimulationTimeout();

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Fake properties that are returned for the emulated device.
  scoped_ptr<Properties> properties_;

  // If true, a pairing simulation was started using BeginPairing() and no call
  // to EndPairing() has been made.
  bool pairing_started_;

  // If true, observers have been notified that a device has been created and
  // the device properties are accessible.
  bool device_visible_;

  // If non-negative, the device will disappear this many milliseconds after
  // its records have been exposed.
  int simulation_timeout_;

  DISALLOW_COPY_AND_ASSIGN(FakeNfcDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_NFC_DEVICE_CLIENT_H_
