// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_NFC_CLIENT_HELPERS_H_
#define CHROMEOS_DBUS_NFC_CLIENT_HELPERS_H_

#include <map>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/nfc_property_set.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"

namespace chromeos {
namespace nfc_client_helpers {

  // Constants used to indicate exceptional error conditions.
CHROMEOS_EXPORT extern const char kNoResponseError[];
CHROMEOS_EXPORT extern const char kUnknownObjectError[];

// The ErrorCallback is used by D-Bus methods to indicate failure.
// It receives two arguments: the name of the error in |error_name| and
// an optional message in |error_message|.
typedef base::Callback<void(const std::string& error_name,
                            const std::string& error_message)> ErrorCallback;


// Called when a response for a successful method call is received.
CHROMEOS_EXPORT void OnSuccess(const base::Closure& callback,
                               dbus::Response* response);

// Extracts the error data from |response| and invokes |error_callback| with
// the resulting error name and error message.
CHROMEOS_EXPORT void OnError(const ErrorCallback& error_callback,
                             dbus::ErrorResponse* response);

// DBusObjectMap is a simple data structure that facilitates keeping track of
// D-Bus object proxies and properties. It maintains a mapping from object
// paths to object proxy - property structure pairs.
// TODO(armansito): This is only needed until neard implements the D-Bus
// org.freedesktop.DBus.ObjectManager interface. Remove this once we upgrade
// to neard-0.14.
class CHROMEOS_EXPORT DBusObjectMap {
 public:
  // DBusObjectMap::Delegate must be implemented by classes that use an
  // instance of DBusObjectMap to manage object proxies.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called by DBusObjectMap to create a Properties structure for the remote
    // D-Bus object accessible through |object_proxy|. The implementation class
    // should create and return an instance of its own subclass of
    // ::chromeos::NfcPropertySet. DBusObjectMap will handle connecting the
    // signals and update the properties.
    virtual NfcPropertySet* CreateProperties(
        dbus::ObjectProxy* object_proxy) = 0;

    // Notifies the delegate that an object was added with object path
    // |object_path|.
    virtual void ObjectAdded(const dbus::ObjectPath& object_path) {}

    // Notifies the delegate that an object was removed with object path
    // |object_path|.
    virtual void ObjectRemoved(const dbus::ObjectPath& object_path) {}
  };

  // Constructor takes in the D-Bus service name the proxies belong to and
  // the delegate which will be used to construct properties structures.
  // |service_name| must be a valid D-Bus service name, and |delegate| cannot
  // be NULL.
  DBusObjectMap(const std::string& service_name,
                Delegate* delegate,
                dbus::Bus* bus);
  virtual ~DBusObjectMap();

  // Returns the object proxy for object path |object_path|. If no object proxy
  // exists for |object_path|, returns NULL.
  dbus::ObjectProxy* GetObjectProxy(const dbus::ObjectPath& object_path);

  // Returns the properties structure for remote object with object path
  // |object_path|. If no properties structure exists for |object_path|,
  // returns NULL.
  NfcPropertySet* GetObjectProperties(const dbus::ObjectPath& object_path);

  // Updates the object proxies from the given list of object paths
  // |object_paths|. It notifies the delegate of each added and removed object
  // via |Delegate::ObjectAdded| and |Delegate::ObjectRemoved|.
  void UpdateObjects(const std::vector<dbus::ObjectPath>& object_paths);

  // Creates and stores an object proxy and properties structure for a remote
  // object with object path |object_path|. If an object proxy was already
  // created, this operation returns false; returns true otherwise.
  bool AddObject(const dbus::ObjectPath& object_path);

  // Removes the D-Bus object proxy and the properties structure for the
  // remote object with object path |object_path|.
  void RemoveObject(const dbus::ObjectPath& object_path);

 private:
  typedef std::pair<dbus::ObjectProxy*, NfcPropertySet*> ObjectPropertyPair;
  typedef std::map<dbus::ObjectPath, ObjectPropertyPair> ObjectMap;

  // Returns an instance of ObjectPropertyPair by looking up |object_path| in
  // |object_map_|. If no entry is found, returns an instance that contains
  // NULL pointers.
  ObjectPropertyPair GetObjectPropertyPair(const dbus::ObjectPath& object_path);

  // Cleans up the object proxy and properties structure in |pair|. This method
  // will remove the object proxy by calling |dbus::Bus::RemoveObjectProxy| and
  // explicitly deallocate the properties structure. Once this method exits,
  // both pointers stored by |pair| will become invalid and should not be used.
  // If |pair| contains invalid pointers at the time when this method is called
  // memory errors are likely to happen, so only valid pointers should be
  // passed.
  void CleanUpObjectPropertyPair(const ObjectPropertyPair& pair);

  dbus::Bus* bus_;
  ObjectMap object_map_;
  std::string service_name_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DBusObjectMap);
};

}  // namespace nfc_client_helpers
}  // namespace chromeos

#endif  // CHROMEOS_DBUS_NFC_CLIENT_HELPERS_H_
