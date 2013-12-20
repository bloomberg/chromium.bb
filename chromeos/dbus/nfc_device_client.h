// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_NFC_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_NFC_DEVICE_CLIENT_H_

#include <map>
#include <string>
#include <vector>

#include "base/values.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "chromeos/dbus/nfc_client_helpers.h"
#include "chromeos/dbus/nfc_property_set.h"
#include "chromeos/dbus/nfc_record_client.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/property.h"

namespace chromeos {

class NfcAdapterClient;

// NfcDeviceClient is used to communicate with objects representing remote NFC
// devices that can be communicated with.
class CHROMEOS_EXPORT NfcDeviceClient : public DBusClient {
 public:
  // Structure of properties associated with an NFC device.
  struct Properties : public NfcPropertySet {
    // List of object paths for NDEF records associated with the NFC device.
    // Read-only.
    dbus::Property<std::vector<dbus::ObjectPath> > records;

    Properties(dbus::ObjectProxy* object_proxy,
               const PropertyChangedCallback& callback);
    virtual ~Properties();
  };

  // Interface for observing changes from a remote NFC device.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when a remote NFC device with the object |object_path| is added
    // to the set of known devices.
    virtual void DeviceAdded(const dbus::ObjectPath& object_path) {}

    // Called when a remote NFC device with the object path |object_path| is
    // removed from the set of known devices.
    virtual void DeviceRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the device property with the name |property_name| on device
    // with object path |object_path| has acquired a new value.
    virtual void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                                       const std::string& property_name) {}
  };

  virtual ~NfcDeviceClient();

  // Adds and removes observers for events on all remote NFC devices. Check the
  // |object_path| parameter of observer methods to determine which device is
  // issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of device object paths associated with the given adapter
  // identified by the D-Bus object path |adapter_path|.
  virtual std::vector<dbus::ObjectPath> GetDevicesForAdapter(
      const dbus::ObjectPath& adapter_path) = 0;

  // Obtain the properties for the NFC device with object path |object_path|;
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Creates an NDEF record for the NFC device with object path |object_path|
  // using the parameters in |attributes|. |attributes| is a dictionary,
  // containing the NFC Record properties which will be assigned to the
  // resulting record object and pushed to the device. The properties are
  // defined by the NFC Record interface (see namespace "nfc_record" in
  // third_party/cros_system_api/dbus/service_constants.h and
  // NfcRecordClient::Properties). |attributes| should at least contain a
  // "Type" plus any other properties associated with that type. For example:
  //
  //    {
  //      "Type": "Text",
  //      "Encoding": "UTF-8",
  //      "Language": "en",
  //      "Representation": "Chrome OS rulez!"
  //    },
  //    {
  //      "Type": "URI",
  //      "URI": "http://www.chromium.org"
  //    },
  //    etc.
  virtual void Push(
      const dbus::ObjectPath& object_path,
      const base::DictionaryValue& attributes,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) = 0;

  // Creates the instance.
  static NfcDeviceClient* Create(NfcAdapterClient* adapter_client);

 protected:
  friend class NfcClientTest;

  NfcDeviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(NfcDeviceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_NFC_DEVICE_CLIENT_H_
