// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_NFC_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_NFC_MANAGER_CLIENT_H_

#include <vector>

#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/nfc_property_set.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace chromeos {

// NfcManagerClient is used to communicate with the neard Manager service.
class CHROMEOS_EXPORT NfcManagerClient : public DBusClient {
 public:
  // Structure of properties associated with the NFC manager.
  struct Properties : public NfcPropertySet {
    // List of Adapter object paths.
    dbus::Property<std::vector<dbus::ObjectPath> > adapters;

    Properties(dbus::ObjectProxy* object_proxy,
               const PropertyChangedCallback& callback);
    virtual ~Properties();
  };

  // Interface for observing changes to the NFC manager. Use this interface
  // to be notified when NFC adapters get added or removed.
  // NOTE: Users of the NFC D-Bus client code shouldn't need to observe changes
  // from NfcManagerClient::Observer; to get notified of changes to the list of
  // NFC adapters, use NfcAdapterClient::Observer instead.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when a new adapter with object path |object_path| is added to the
    // system.
    virtual void AdapterAdded(const dbus::ObjectPath& object_path) {}

    // Called when an adapter with object path |object_path| is removed from the
    // system.
    virtual void AdapterRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the manager property with name |property_name| has acquired
    // a new value.
    virtual void ManagerPropertyChanged(const std::string& property_name) {}
  };

  virtual ~NfcManagerClient();

  // Adds and removes observers for events on the NFC manager.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Obtains the properties of the NFC manager service.
  virtual Properties* GetProperties() = 0;

  // Creates the instance.
  static NfcManagerClient* Create();

 protected:
  friend class NfcClientTest;

  NfcManagerClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(NfcManagerClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_NFC_MANAGER_CLIENT_H_
