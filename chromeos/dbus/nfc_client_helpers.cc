// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/nfc_client_helpers.h"

#include "base/stl_util.h"
#include "dbus/values_util.h"

namespace chromeos {
namespace nfc_client_helpers {

const char kNoResponseError[] = "org.chromium.Error.NoResponse";
const char kUnknownObjectError[] = "org.chromium.Error.UnknownObject";

void OnSuccess(const base::Closure& callback, dbus::Response* response) {
  DCHECK(response);
  callback.Run();
}

void OnError(const ErrorCallback& error_callback,
             dbus::ErrorResponse* response) {
  // Error response has optional error message argument.
  std::string error_name;
  std::string error_message;
  if (response) {
    dbus::MessageReader reader(response);
    error_name = response->GetErrorName();
    reader.PopString(&error_message);
  } else {
    error_name = kNoResponseError;
    error_message = "";
  }
  error_callback.Run(error_name, error_message);
}

DBusObjectMap::DBusObjectMap(const std::string& service_name,
                             Delegate* delegate,
                             dbus::Bus* bus)
    : bus_(bus),
      service_name_(service_name),
      delegate_(delegate) {
  DCHECK(bus_);
  DCHECK(delegate_);
}

DBusObjectMap::~DBusObjectMap() {
  // Clean up the Properties structures. We don't explicitly delete the object
  // proxies, as they are owned by dbus::Bus.
  for (ObjectMap::iterator iter = object_map_.begin();
       iter != object_map_.end(); ++iter) {
    const dbus::ObjectPath& object_path = iter->first;
    ObjectPropertyPair pair = iter->second;
    delegate_->ObjectRemoved(object_path);
    CleanUpObjectPropertyPair(pair);
  }
}

dbus::ObjectProxy* DBusObjectMap::GetObjectProxy(
    const dbus::ObjectPath& object_path) {
  return GetObjectPropertyPair(object_path).first;
}

NfcPropertySet* DBusObjectMap::GetObjectProperties(
    const dbus::ObjectPath& object_path) {
  return GetObjectPropertyPair(object_path).second;
}

void DBusObjectMap::UpdateObjects(const ObjectPathVector& object_paths) {
  // This set is used to query if an object path was removed, while updating
  // the removed paths below. The iterator is used as a hint to accelerate
  // insertion.
  std::set<dbus::ObjectPath> object_path_set;
  std::set<dbus::ObjectPath>::iterator object_path_set_iter =
      object_path_set.begin();

  // Add all objects.
  for (ObjectPathVector::const_iterator iter = object_paths.begin();
       iter != object_paths.end(); ++iter) {
    const dbus::ObjectPath &object_path = *iter;
    AddObject(object_path);
    // Neard usually returns object paths in ascending order. Give a hint here
    // to make insertion amortized constant.
    object_path_set_iter =
        object_path_set.insert(object_path_set_iter, object_path);
  }

  // Remove all objects that are not in |object_paths|.
  ObjectMap::const_iterator iter = object_map_.begin();
  while (iter != object_map_.end()) {
    // It is safe to use a const reference here, as DBusObjectMap::RemoveObject
    // won't access it after the iterator becomes invalidated.
    const dbus::ObjectPath &object_path = iter->first;
    ++iter;
    if (!ContainsKey(object_path_set, object_path))
      RemoveObject(object_path);
  }
}

bool DBusObjectMap::AddObject(const dbus::ObjectPath& object_path) {
  ObjectMap::iterator iter = object_map_.find(object_path);
  if (iter != object_map_.end())
    return false;

  DCHECK(bus_);
  DCHECK(delegate_);
  dbus::ObjectProxy* object_proxy = bus_->GetObjectProxy(service_name_,
                                                         object_path);

  // Create the properties structure.
  NfcPropertySet* properties = delegate_->CreateProperties(object_proxy);
  properties->ConnectSignals();
  properties->GetAll();
  ObjectPropertyPair object = std::make_pair(object_proxy, properties);
  object_map_[object_path] = object;
  VLOG(1) << "Created proxy for object with Path: " << object_path.value()
          << ", Service: " << service_name_;
  delegate_->ObjectAdded(object_path);
  return true;
}

void DBusObjectMap::RemoveObject(const dbus::ObjectPath& object_path) {
  DCHECK(bus_);
  DCHECK(delegate_);
  ObjectMap::iterator iter = object_map_.find(object_path);
  if (iter == object_map_.end())
    return;

  // Notify the delegate, so that it can perform any clean up that is
  // necessary.
  delegate_->ObjectRemoved(object_path);

  // Clean up the object proxy and the properties structure.
  ObjectPropertyPair pair = iter->second;
  CleanUpObjectPropertyPair(pair);
  object_map_.erase(iter);
  VLOG(1) << "Object proxy removed for object path: "
          << object_path.value();
}

void DBusObjectMap::RefreshProperties(const dbus::ObjectPath& object_path) {
  NfcPropertySet* properties = GetObjectProperties(object_path);
  if (properties)
    properties->GetAll();
}

void DBusObjectMap::RefreshAllProperties() {
  for (ObjectMap::const_iterator iter = object_map_.begin();
       iter != object_map_.end(); ++iter) {
    const dbus::ObjectPath& object_path = iter->first;
    RefreshProperties(object_path);
  }
}

ObjectPathVector DBusObjectMap::GetObjectPaths() {
  std::vector<dbus::ObjectPath> object_paths;
  for (ObjectMap::const_iterator iter = object_map_.begin();
       iter != object_map_.end(); ++iter) {
    const dbus::ObjectPath& object_path = iter->first;
    object_paths.push_back(object_path);
  }
  return object_paths;
}

DBusObjectMap::ObjectPropertyPair DBusObjectMap::GetObjectPropertyPair(
    const dbus::ObjectPath& object_path) {
  ObjectMap::iterator iter = object_map_.find(object_path);
  if (iter != object_map_.end())
    return iter->second;
  return std::make_pair(static_cast<dbus::ObjectProxy*>(NULL),
                        static_cast<NfcPropertySet*>(NULL));
}

void DBusObjectMap::CleanUpObjectPropertyPair(const ObjectPropertyPair& pair) {
  // Don't remove the object proxy. There is a bug in dbus::Bus that causes a
  // crash when object proxies are removed, due to the proxy objects not being
  // properly reference counted. (See crbug.com/170182 and crbug.com/328264).
  NfcPropertySet* properties = pair.second;
  delete properties;
}

ObjectProxyTree::ObjectProxyTree() {
}

ObjectProxyTree::~ObjectProxyTree() {
  for (PathsToObjectMapsType::iterator iter = paths_to_object_maps_.begin();
       iter != paths_to_object_maps_.end(); ++iter) {
    DBusObjectMap* object_map = iter->second;
    delete object_map;
  }
}

bool ObjectProxyTree::CreateObjectMap(const dbus::ObjectPath& object_path,
                                      const std::string& service_name,
                                      DBusObjectMap::Delegate* delegate,
                                      dbus::Bus* bus) {
  if (ContainsKey(paths_to_object_maps_, object_path)) {
    LOG(ERROR) << "Mapping already exists for object path: "
               << object_path.value();
    return false;
  }
  paths_to_object_maps_[object_path] =
      new DBusObjectMap(service_name, delegate, bus);
  return true;
}

void ObjectProxyTree::RemoveObjectMap(const dbus::ObjectPath& object_path) {
  PathsToObjectMapsType::iterator iter =
      paths_to_object_maps_.find(object_path);
  if (iter == paths_to_object_maps_.end())
    return;
  DBusObjectMap* object_map = iter->second;
  paths_to_object_maps_.erase(iter);
  delete object_map;
}

DBusObjectMap* ObjectProxyTree::GetObjectMap(
    const dbus::ObjectPath& object_path) {
  PathsToObjectMapsType::iterator iter =
      paths_to_object_maps_.find(object_path);
  if (iter == paths_to_object_maps_.end())
    return NULL;
  return iter->second;
}

dbus::ObjectProxy* ObjectProxyTree::FindObjectProxy(
    const dbus::ObjectPath& object_proxy_path) {
  for (PathsToObjectMapsType::iterator iter = paths_to_object_maps_.begin();
       iter != paths_to_object_maps_.end(); ++iter) {
    DBusObjectMap* object_map = iter->second;
    dbus::ObjectProxy* object_proxy =
        object_map->GetObjectProxy(object_proxy_path);
    if (object_proxy)
      return object_proxy;
  }
  return NULL;
}

NfcPropertySet* ObjectProxyTree::FindObjectProperties(
    const dbus::ObjectPath& object_proxy_path) {
  for (PathsToObjectMapsType::iterator iter = paths_to_object_maps_.begin();
       iter != paths_to_object_maps_.end(); ++iter) {
    DBusObjectMap* object_map = iter->second;
    NfcPropertySet* properties =
        object_map->GetObjectProperties(object_proxy_path);
    if (properties)
      return properties;
  }
  return NULL;
}

}  // namespace nfc_client_helpers
}  // namespace chromeos
