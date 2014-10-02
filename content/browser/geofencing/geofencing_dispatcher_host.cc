// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geofencing/geofencing_dispatcher_host.h"

#include "content/common/geofencing_messages.h"
#include "content/common/geofencing_status.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"

namespace content {

static const int kMaxRegionIdLength = 200;

GeofencingDispatcherHost::GeofencingDispatcherHost()
    : BrowserMessageFilter(GeofencingMsgStart) {
}

GeofencingDispatcherHost::~GeofencingDispatcherHost() {
}

bool GeofencingDispatcherHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GeofencingDispatcherHost, message)
  IPC_MESSAGE_HANDLER(GeofencingHostMsg_RegisterRegion, OnRegisterRegion)
  IPC_MESSAGE_HANDLER(GeofencingHostMsg_UnregisterRegion, OnUnregisterRegion)
  IPC_MESSAGE_HANDLER(GeofencingHostMsg_GetRegisteredRegions,
                      OnGetRegisteredRegions)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GeofencingDispatcherHost::OnRegisterRegion(
    int thread_id,
    int request_id,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region) {
  // Sanity check on region_id
  if (region_id.length() > kMaxRegionIdLength) {
    Send(new GeofencingMsg_RegisterRegionComplete(
        thread_id, request_id, GeofencingStatus::GEOFENCING_STATUS_ERROR));
    return;
  }
  // TODO(mek): Handle registration request.
  Send(new GeofencingMsg_RegisterRegionComplete(
      thread_id,
      request_id,
      GeofencingStatus::
          GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE));
}

void GeofencingDispatcherHost::OnUnregisterRegion(
    int thread_id,
    int request_id,
    const std::string& region_id) {
  // Sanity check on region_id
  if (region_id.length() > kMaxRegionIdLength) {
    Send(new GeofencingMsg_UnregisterRegionComplete(
        thread_id, request_id, GeofencingStatus::GEOFENCING_STATUS_ERROR));
    return;
  }
  // TODO(mek): Handle unregistration request.
  Send(new GeofencingMsg_UnregisterRegionComplete(
      thread_id,
      request_id,
      GeofencingStatus::
          GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE));
}

void GeofencingDispatcherHost::OnGetRegisteredRegions(int thread_id,
                                                      int request_id) {
  GeofencingRegistrations result;
  Send(new GeofencingMsg_GetRegisteredRegionsComplete(
      thread_id,
      request_id,
      GeofencingStatus::
          GEOFENCING_STATUS_OPERATION_FAILED_SERVICE_NOT_AVAILABLE,
      result));
}

}  // namespace content
