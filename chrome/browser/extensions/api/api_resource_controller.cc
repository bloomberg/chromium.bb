// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/api_resource_controller.h"
#include "chrome/browser/extensions/api/serial/serial_connection.h"
#include "chrome/browser/extensions/api/socket/socket.h"
#include "chrome/browser/extensions/api/usb/usb_device_resource.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

APIResourceController::APIResourceController()
  : next_api_resource_id_(1),
    socket_resource_map_(new APIResourceMap),
    serial_connection_resource_map_(new APIResourceMap),
    usb_device_resource_map_(new APIResourceMap) {}

APIResourceController::~APIResourceController() {
  // If this check failed, then a unit test was using an APIResource but
  // didn't provide the IO/FILE thread message loops needed for those resources
  // to do their job (including destroying themselves at shutdown).
  DCHECK(BrowserThread::IsMessageLoopValid(BrowserThread::IO));
  DCHECK(BrowserThread::IsMessageLoopValid(BrowserThread::FILE));

  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE,
    socket_resource_map_);
  BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE,
    serial_connection_resource_map_);
  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE,
    usb_device_resource_map_);
}

APIResourceController::APIResourceMap*
APIResourceController::GetResourceMapForType(
  APIResource::APIResourceType api_resource_type) const {
    switch (api_resource_type) {
      case APIResource::SocketResource:
        return socket_resource_map_;
      case APIResource::SerialConnectionResource:
        return serial_connection_resource_map_;
      case APIResource::UsbDeviceResource:
        return usb_device_resource_map_;
      default:
        NOTREACHED();
        return NULL;
    }
}

APIResource* APIResourceController::GetAPIResource(
    APIResource::APIResourceType api_resource_type,
    int api_resource_id) const {
  // TODO(miket): verify that the extension asking for the APIResource is the
  // same one that created it.
  APIResourceMap* map = GetResourceMapForType(api_resource_type);
  APIResourceMap::const_iterator i = map->find(api_resource_id);
  if (i == map->end())
    return NULL;

  APIResource* resource = i->second.get();

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
  APIResourceMap* map = GetResourceMapForType(
    api_resource->api_resource_type());
  int id = GenerateAPIResourceId();
  if (id > 0) {
    linked_ptr<APIResource> resource_ptr(api_resource);
    (*map)[id] = resource_ptr;
    return id;
  }
  return 0;
}

bool APIResourceController::RemoveAPIResource(
  APIResource::APIResourceType api_resource_type, int api_resource_id) {
  APIResource* api_resource = GetAPIResource(api_resource_type,
    api_resource_id);
  if (!api_resource)
    return false;
  APIResourceMap* map = GetResourceMapForType(
    api_resource->api_resource_type());
  map->erase(api_resource_id);
  return true;
}

bool APIResourceController::RemoveSocket(int api_resource_id) {
  return RemoveAPIResource(APIResource::SocketResource, api_resource_id);
}

bool APIResourceController::RemoveSerialConnection(int api_resource_id) {
  return RemoveAPIResource(APIResource::SerialConnectionResource,
    api_resource_id);
}

bool APIResourceController::RemoveUsbDeviceResource(int api_resource_id) {
  return RemoveAPIResource(APIResource::UsbDeviceResource, api_resource_id);
}

// TODO(miket): consider partitioning the ID space by extension ID
// to make it harder for extensions to peek into each others' resources.
int APIResourceController::GenerateAPIResourceId() {
  return next_api_resource_id_++;
}

Socket* APIResourceController::GetSocket(int api_resource_id) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return static_cast<Socket*>(GetAPIResource(APIResource::SocketResource,
                                             api_resource_id));
}

SerialConnection* APIResourceController::GetSerialConnection(
    int api_resource_id) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return static_cast<SerialConnection*>(
      GetAPIResource(APIResource::SerialConnectionResource, api_resource_id));
}

UsbDeviceResource* APIResourceController::GetUsbDeviceResource(
    int api_resource_id) const {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  return static_cast<UsbDeviceResource*>(GetAPIResource(
      APIResource::UsbDeviceResource, api_resource_id));
}

}  // namespace extensions
