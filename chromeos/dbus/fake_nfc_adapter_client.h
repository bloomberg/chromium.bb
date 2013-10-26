// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_NFC_ADAPTER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_NFC_ADAPTER_CLIENT_H_

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/nfc_adapter_client.h"
#include "chromeos/dbus/nfc_client_helpers.h"

namespace chromeos {

// FakeNfcAdapterClient simulates the behavior of the NFC adapter objects
// and is used both in test cases in place of a mock and on the Linux desktop.
// TODO(armansito): For now, this doesn't do anything. Implement fake
// behavior in conjunction with unit tests while implementing the src/device
// layer.
class CHROMEOS_EXPORT FakeNfcAdapterClient : public NfcAdapterClient {
 public:
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
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path)
      OVERRIDE;
  virtual void StartPollLoop(
      const dbus::ObjectPath& object_path,
      const std::string& mode,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) OVERRIDE;
  virtual void StopPollLoop(
      const dbus::ObjectPath& object_path,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) OVERRIDE;

 private:
  // Property callback passed when we create Properties* structures.
  void OnPropertyChanged(const std::string& property_name);

  DISALLOW_COPY_AND_ASSIGN(FakeNfcAdapterClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_NFC_ADAPTER_CLIENT_H_
