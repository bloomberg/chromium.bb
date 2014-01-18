// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_NFC_TAG_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_NFC_TAG_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/nfc_client_helpers.h"
#include "chromeos/dbus/nfc_tag_client.h"

namespace chromeos {

// FakeNfcTagClient simulates the behavior of the NFC tag objects
// and is used both in test cases in place of a mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeNfcTagClient : public NfcTagClient {
 public:
  // The fake tag object path.
  static const char kTagPath[];

  // The default simulation timeout interval.
  static const int kDefaultSimulationTimeoutMilliseconds;

  struct Properties : public NfcTagClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // dbus::PropertySet overrides.
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeNfcTagClient();
  virtual ~FakeNfcTagClient();

  // NfcTagClient overrides.
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual std::vector<dbus::ObjectPath> GetTagsForAdapter(
      const dbus::ObjectPath& adapter_path) OVERRIDE;
  virtual Properties* GetProperties(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void Write(
      const dbus::ObjectPath& object_path,
      const base::DictionaryValue& attributes,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) OVERRIDE;

  // Simulates the appearance of a tag. The fake tag will show up after
  // exactly |visibility_delay| milliseconds. |visibility_delay| must have a
  // non-negative value. The side-effects of this method
  // occur asynchronously, i.e. even with an argument of 0, the pairing will not
  // take place until after this method has returned.
  void BeginPairingSimulation(int visibility_delay);

  // If tag pairing was previously started, simulates the disappearance of
  // the tag. Any tag object presented and their records will disappear
  // after this call. Delayed events that were set up by a previous call to
  // BeginPairing() will be canceled through a call to EndPairing().
  void EndPairingSimulation();

  // Enables or disables automatic unpairing. When enabled, a pairing
  // simulation will end |simulation_timeout| milliseconds after the tag has
  // been exposed. This is enabled by default and the timeout is set to
  // |kDefaultSimulationTimeoutMilliseconds|. |simulation_timeout| must be
  // non-negative.
  void EnableSimulationTimeout(int simulation_timeout);
  void DisableSimulationTimeout();

  // Tells the FakeNfcDeviceClient to add the records in |record_paths| to its
  // list of records exposed for |kDevicePath|. This method will immediately
  // assign the records and trigger a property changed signal, only if the
  // tag is currently visible.
  void SetRecords(const std::vector<dbus::ObjectPath>& record_paths);

  // Tells the FakeNfcDeviceClient to clear the list of records exposed for
  // |kDevicePath|. This method takes effect immediately and triggers a
  // property changed signal.
  void ClearRecords();

  // Returns true, if a pairing simulation is currently going on.
  bool tag_visible() const { return tag_visible_; }

 private:
  // Property changed callback passed when we create Properties* structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // Makes the fake tag visible if it is not already visible.
  void MakeTagVisible();

  // Called when the simulation timeout expires.
  void HandleSimulationTimeout();

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Fake properties that are returned for the emulated tag.
  scoped_ptr<Properties> properties_;

  // If true, a pairing simulation was begun using BeginPairing() and no call
  // to EndPairing() has been made.
  bool pairing_started_;

  // If true, observers have been notified that a tag has been created and
  // the tag properties are accesible.
  bool tag_visible_;

  // If non-negative, the tag will disappear this many milliseconds after
  // its records have been exposed.
  int simulation_timeout_;

  DISALLOW_COPY_AND_ASSIGN(FakeNfcTagClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_NFC_TAG_CLIENT_H_
