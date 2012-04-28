// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/api_resource_controller.h"
#include "chrome/browser/extensions/api/serial/serial_connection.h"
#include "chrome/browser/extensions/api/socket/socket.h"
#include "chrome/browser/extensions/api/usb/usb_device_resource.h"

namespace extensions {

APIResourceController::APIResourceController() : next_api_resource_id_(1) {}

APIResourceController::~APIResourceController() {}

APIResource* APIResourceController::GetAPIResource(int api_resource_id) const {
  // TODO(miket): verify that the extension asking for the APIResource is the
  // same one that created it.
  APIResourceMap::const_iterator i = api_resource_map_.find(api_resource_id);
  if (i != api_resource_map_.end())
    return i->second.get();
  return NULL;
}

APIResource* APIResourceController::GetAPIResource(
    APIResource::APIResourceType api_resource_type,
    int api_resource_id) const {
  APIResource* resource = GetAPIResource(api_resource_id);

  // This DCHECK is going to catch some legitimate application-developer
  // errors, where someone asks for resource of Type A with the wrong ID that
  // happens to belong to a resource of Type B. But in debug mode, it's more
  // likely to catch a silly copy/paste coding error while developing a new
  // resource type, and unfortunately we can't really tell the difference.
  // We'll choose the more informative outcome (loud explosion during debug)
  // rather than a mysterious empty resource returned to JavaScript.
  DCHECK(!resource || resource->api_resource_type() == api_resource_type);

  if (resource && resource->api_resource_type() != api_resource_type)
    resource = NULL;

  return resource;
}

int APIResourceController::AddAPIResource(APIResource* api_resource) {
  int id = GenerateAPIResourceId();
  if (id > 0) {
    linked_ptr<APIResource> resource_ptr(api_resource);
    api_resource_map_[id] = resource_ptr;
    return id;
  }
  return 0;
}

bool APIResourceController::RemoveAPIResource(int api_resource_id) {
  APIResource* api_resource = GetAPIResource(api_resource_id);
  if (!api_resource)
    return false;
  api_resource_map_.erase(api_resource_id);
  return true;
}

// TODO(miket): consider partitioning the ID space by extension ID
// to make it harder for extensions to peek into each others' resources.
int APIResourceController::GenerateAPIResourceId() {
  while (next_api_resource_id_ > 0 &&
         api_resource_map_.count(next_api_resource_id_) > 0)
    ++next_api_resource_id_;
  return next_api_resource_id_++;
}

Socket* APIResourceController::GetSocket(int api_resource_id) const {
  return static_cast<Socket*>(GetAPIResource(APIResource::SocketResource,
                                             api_resource_id));
}

SerialConnection* APIResourceController::GetSerialConnection(
    int api_resource_id) const {
  return static_cast<SerialConnection*>(
      GetAPIResource(APIResource::SerialConnectionResource, api_resource_id));
}

UsbDeviceResource* APIResourceController::GetUsbDeviceResource(
    int api_resource_id) const {
  return static_cast<UsbDeviceResource*>(GetAPIResource(
      APIResource::UsbDeviceResource, api_resource_id));
}

}  // namespace extensions
