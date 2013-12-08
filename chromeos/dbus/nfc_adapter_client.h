// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_NFC_ADAPTER_CLIENT_H_
#define CHROMEOS_DBUS_NFC_ADAPTER_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/nfc_client_helpers.h"
#include "chromeos/dbus/nfc_property_set.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace chromeos {

class NfcManagerClient;

// NfcAdapterClient is used to communicate with objects representing local NFC
// adapters.
class CHROMEOS_EXPORT NfcAdapterClient : public DBusClient {
 public:
  // Structure of properties associated with an NFC adapter.
  struct Properties : public NfcPropertySet {
    // The adapter NFC radio mode. One of "Initiator", "Target", and "Idle".
    // The NFC adapter will usually be in the "Idle" mode. The mode will change
    // to "Initiator" or "Target" based on how a pairing is established with a
    // remote tag or device. Read-only.
    dbus::Property<std::string> mode;

    // The adapter's current power state. Read-write.
    dbus::Property<bool> powered;

    // Indicates whether or not the adapter is currently polling for targets.
    // This property is only valid when |mode| is "Initiator". Read-only.
    dbus::Property<bool> polling;

    // The NFC protocols that are supported by the adapter. Possible values
    // are: "Felica", "MIFARE", "Jewel", "ISO-DEP", and "NFC-DEP". Read-only.
    dbus::Property<std::vector<std::string> > protocols;

    // The object paths of the NFC tags that are known to the local adapter.
    // These are tags that have been "tapped" on the local adapter. Read-only.
    dbus::Property<std::vector<dbus::ObjectPath> > tags;

    // The object paths of the remote NFC devices that have been found by the
    // local adapter. These are NFC adapters that were "tapped" on the local
    // adapter. Read-only.
    dbus::Property<std::vector<dbus::ObjectPath> > devices;

    Properties(dbus::ObjectProxy* object_proxy,
               const PropertyChangedCallback& callback);
    virtual ~Properties();
  };

  // Interface for observing changes from a local NFC adapter.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when a new adapter with object path |object_path| is added to the
    // system.
    virtual void AdapterAdded(const dbus::ObjectPath& object_path) {}

    // Called when an adapter with object path |object_path| is removed from the
    // system.
    virtual void AdapterRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the adapter property with the name |property_name| on adapter
    // with object path |object_path| has acquired a new value.
    virtual void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                                        const std::string& property_name) {}
  };

  virtual ~NfcAdapterClient();

  // Adds and removes observers for events on all local bluetooth adapters.
  // Check the |object_path| parameter of the observer methods to determine
  // which adapter is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of adapter object paths known to the system.
  virtual std::vector<dbus::ObjectPath> GetAdapters() = 0;

  // Obtains the properties for the adapter with object path |object_path|, any
  // values should be copied if needed. A NULL pointer will be returned, if no
  // adapter with the given object path is known to exist.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Starts the polling loop for the adapter with object path |object_path|.
  // Depending on the mode, the adapter will start polling for targets,
  // listening to NFC devices, or both. The |mode| parameter should be one of
  // "Initiator", "Target", or "Dual". The "Dual" mode will have the adapter
  // alternate between "Initiator" and "Target" modes during the polling loop.
  // For any other value, the adapter will default to "Initiator" mode.
  virtual void StartPollLoop(
      const dbus::ObjectPath& object_path,
      const std::string& mode,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) = 0;

  // Stops the polling loop for the adapter with object_path |object_path|.
  virtual void StopPollLoop(
      const dbus::ObjectPath& object_path,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) = 0;

  // Creates the instance.
  static NfcAdapterClient* Create(NfcManagerClient* manager_client);

 protected:
  friend class NfcClientTest;

  NfcAdapterClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(NfcAdapterClient);
};

}  // namespace chromeos

#endif   // CHROMEOS_DBUS_NFC_ADAPTER_CLIENT_H_
