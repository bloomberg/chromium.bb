// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_NFC_TAG_CLIENT_H_
#define CHROMEOS_DBUS_NFC_TAG_CLIENT_H_

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

// NfcTagClient is used to communicate with objects representing remote NFC
// tags.
class CHROMEOS_EXPORT NfcTagClient : public DBusClient {
 public:
  // Structure of properties associated with an NFC tag.
  struct Properties : public NfcPropertySet {
    // The NFC tag type. Possible values are "Type 1", "Type 2", "Type 3",
    // and "Type 4". Read-only.
    dbus::Property<std::string> type;

    // The NFC tag radio protocol. Possible values are "Felica", "MIFARE",
    // "Jewel", "ISO-DEP", and "NFC-DEP". Read-only.
    dbus::Property<std::string> protocol;

    // List of object paths for NDEF Records associated with the NFC tag.
    // Read-only.
    dbus::Property<std::vector<dbus::ObjectPath> > records;

    // The current status of the tag's read mode. Read-only.
    dbus::Property<bool> read_only;

    Properties(dbus::ObjectProxy* object_proxy,
               const PropertyChangedCallback& callback);
    virtual ~Properties();
  };

  // Interface for observing changes from a remote NFC tag.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when a remote NFC tag with the object path |object_path| is added
    // to the set of known tags.
    virtual void TagAdded(const dbus::ObjectPath& object_path) {}

    // Called when a remote NFC tag with the object path |object_path| is
    // removed from the set of known tags.
    virtual void TagRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the tag property with the name |property_name| on tag with
    // object path |object_path| has acquired a new value.
    virtual void TagPropertyChanged(const dbus::ObjectPath& object_path,
                                    const std::string& property_name) {}

    // Called when all properties for the tag with object path |object_path|
    // have been received. This method will be called after
    // Observer::TagPropertyChanged has been called for all properties that
    // were received through the initial property fetch that is done when the
    // object proxy is first created or after a call to
    // dbus::PropertySet::GetAll. Observers can use this method to be notified
    // when all existing properties of a tag are available for use.
    virtual void TagPropertiesReceived(const dbus::ObjectPath& object_path) {}
  };

  virtual ~NfcTagClient();

  // Adds and removes observers for events on all remote NFC tags. Check the
  // |object_path| parameter of observer methods to determine which tag is
  // issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of tag object paths associated with the given adapter
  // identified by the D-Bus object path |adapter_path|.
  virtual std::vector<dbus::ObjectPath> GetTagsForAdapter(
      const dbus::ObjectPath& adapter_path) = 0;

  // Obtain the properties for the NFC tag with object path |object_path|; any
  // values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Creates an NDEF record for the NFC tag with object path |object_path|
  // using the parameters in |attributes|. |attributes| is a dictionary,
  // containing the NFC Record properties which will be assigned to the
  // resulting record object and written to the tag. The properties are defined
  // by the NFC Record interface (see namespace "nfc_record" in
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
  virtual void Write(
      const dbus::ObjectPath& object_path,
      const base::DictionaryValue& attributes,
      const base::Closure& callback,
      const nfc_client_helpers::ErrorCallback& error_callback) = 0;

  // Creates the instance.
  static NfcTagClient* Create(NfcAdapterClient* adapter_client);

 protected:
  friend class NfcClientTest;

  NfcTagClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(NfcTagClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_NFC_TAG_CLIENT_H_
