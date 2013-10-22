// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_NFC_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_NFC_MANAGER_CLIENT_H_

#include <set>
#include <string>

#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/nfc_manager_client.h"
#include "dbus/property.h"

namespace chromeos {

// FakeNfcManagerClient simulates the behavior of the NFC Daemon manager object
// and is used both in test cases in place of a mock and on the Linux desktop.
class CHROMEOS_EXPORT FakeNfcManagerClient : public NfcManagerClient {
 public:
  struct Properties : public NfcManagerClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    virtual ~Properties();

    // dbus::PropertySet overrides.
    virtual void Get(dbus::PropertyBase* property,
                     dbus::PropertySet::GetCallback callback) OVERRIDE;
    virtual void GetAll() OVERRIDE;
    virtual void Set(dbus::PropertyBase* property,
                     dbus::PropertySet::SetCallback callback) OVERRIDE;
  };

  FakeNfcManagerClient();
  virtual ~FakeNfcManagerClient();

  // NfcManagerClient overrides.
  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual Properties* GetProperties() OVERRIDE;

  // Methods to simulate adapters getting added and removed.
  void AddAdapter(const std::string& adapter_path);
  void RemoveAdapter(const std::string& adapter_path);

  // Default path of an adapter that is simulated for testing.
  static const char kDefaultAdapterPath[];

 private:
  // Property callback passed when we create Properties* structures.
  void OnPropertyChanged(const std::string& property_name);

  // List of observers interested in event notifications.
  ObserverList<Observer> observers_;

  // Set containing the currently simulated adapters.
  std::set<dbus::ObjectPath> adapters_;

  // Fake properties object. This gets updated whenever AddAdapter or
  // RemoveAdapter gets called.
  scoped_ptr<Properties> properties_;

  DISALLOW_COPY_AND_ASSIGN(FakeNfcManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_NFC_MANAGER_CLIENT_H_
