// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_NFC_ADAPTER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_NFC_ADAPTER_CLIENT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/nfc_adapter_client.h"
#include "chromeos/dbus/nfc_client_helpers.h"

namespace chromeos {

// FakeNfcAdapterClient simulates the behavior of the NFC adapter objects
// and is used both in test cases in place of a mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeNfcAdapterClient : public NfcAdapterClient {
 public:
  // The object paths for the adapters that are being emulated.
  static const char kAdapterPath0[];
  static const char kAdapterPath1[];

  // Properties structure that provides fake behavior for D-Bus calls.
  struct Properties : public NfcAdapterClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // dbus::PropertySet overrides.
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeNfcAdapterClient();
  virtual ~FakeNfcAdapterClient();

  // NfcAdapterClient overrides.
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual std::vector<dbus::ObjectPath> GetAdapters() OVERRIDE;
  virtual Properties* GetProperties(
      const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void StartPollLoop(
      const dbus::ObjectPath& object_path,
      const std::string& mode,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) OVERRIDE;
  virtual void StopPollLoop(
      const dbus::ObjectPath& object_path,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) OVERRIDE;

  // Sets the adapter as |present|. Used for testing.
  void SetAdapterPresent(bool present);
  void SetSecondAdapterPresent(bool present);

  // Tells the FakeNfcAdapterClient to add the device or tag with the given path
  // to its corresponding list for |kAdapterPath0|, if it is not already in
  // the list and promptly triggers a property changed signal. This method will
  // also fail, if the polling property of the adapter is false and will set it
  // to false on success.
  void SetDevice(const dbus::ObjectPath& device_path);
  void SetTag(const dbus::ObjectPath& tag_path);

  // Tells the FakeNfcAdapterClient to remove the device or tag with the given
  // path from its corresponding list exposed for |kAdapterPath0|, if it
  // is in the list. On success, this method will mark the polling property of
  // the adapter to true.
  void UnsetDevice(const dbus::ObjectPath& device_path);
  void UnsetTag(const dbus::ObjectPath& tag_path);

  // Sets a flag that determines whether FakeNfcAdapterClient should notify
  // FakeNfcDeviceClient or FakeNfcTagClient to start a pairing simulation as a
  // result of a call to StartPollLoop(). This is enabled by default. If
  // enabled, the first call to StartPollLoop, will initiate a tag pairing
  // simulation. The simulation will alternate between device and tag pairing on
  // each successive call to StartPollLoop. This behavior, which is meant for
  // feature development based on fake classes, can be disabled to allow manual
  // control for unit tests.
  void EnablePairingOnPoll(bool enabled);

 private:
  // Property changed callback passed when we create Properties* structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // List of observers interested in event notifications from us.
  ObserverList<Observer> observers_;

  // Fake properties that are returned for the emulated adapters.
  scoped_ptr<Properties> properties_;
  scoped_ptr<Properties> second_properties_;

  // Whether the adapter and second adapter are present or not.
  bool present_;
  bool second_present_;

  // If true, a pairing simulation is initiated on a successful call to
  // StartPollLoop().
  bool start_pairing_on_poll_;

  // If true, device pairing will be simulated on the next call to
  // StartPollLoop. Otherwise, tag pairing will be simulated.
  bool device_pairing_;

  DISALLOW_COPY_AND_ASSIGN(FakeNfcAdapterClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_NFC_ADAPTER_CLIENT_H_
