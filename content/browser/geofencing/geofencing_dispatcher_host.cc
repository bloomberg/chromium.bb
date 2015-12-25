// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geofencing/geofencing_dispatcher_host.h"

#include "content/browser/geofencing/geofencing_manager.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/geofencing_messages.h"
#include "third_party/WebKit/public/platform/WebCircularGeofencingRegion.h"

namespace content {

static const int kMaxRegionIdLength = 200;

GeofencingDispatcherHost::GeofencingDispatcherHost(
    GeofencingManager* geofencing_manager)
    : BrowserMessageFilter(GeofencingMsgStart),
      manager_(geofencing_manager),
      weak_factory_(this) {
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
  IPC_MESSAGE_FORWARD(GeofencingHostMsg_SetMockProvider, manager_.get(),
                      GeofencingManager::SetMockProvider)
  IPC_MESSAGE_FORWARD(GeofencingHostMsg_SetMockPosition, manager_.get(),
                      GeofencingManager::SetMockPosition)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GeofencingDispatcherHost::OnRegisterRegion(
    int thread_id,
    int request_id,
    const std::string& region_id,
    const blink::WebCircularGeofencingRegion& region,
    int64_t service_worker_registration_id) {
  // Sanity check on region_id
  if (region_id.length() > kMaxRegionIdLength) {
    Send(new GeofencingMsg_RegisterRegionComplete(
        thread_id, request_id, GeofencingStatus::GEOFENCING_STATUS_ERROR));
    return;
  }

  manager_->RegisterRegion(
      service_worker_registration_id,
      region_id,
      region,
      base::Bind(&GeofencingDispatcherHost::RegisterRegionCompleted,
                 weak_factory_.GetWeakPtr(),
                 thread_id,
                 request_id));
}

void GeofencingDispatcherHost::OnUnregisterRegion(
    int thread_id,
    int request_id,
    const std::string& region_id,
    int64_t service_worker_registration_id) {
  // Sanity check on region_id
  if (region_id.length() > kMaxRegionIdLength) {
    Send(new GeofencingMsg_UnregisterRegionComplete(
        thread_id, request_id, GeofencingStatus::GEOFENCING_STATUS_ERROR));
    return;
  }

  manager_->UnregisterRegion(
      service_worker_registration_id,
      region_id,
      base::Bind(&GeofencingDispatcherHost::UnregisterRegionCompleted,
                 weak_factory_.GetWeakPtr(),
                 thread_id,
                 request_id));
}

void GeofencingDispatcherHost::OnGetRegisteredRegions(
    int thread_id,
    int request_id,
    int64_t service_worker_registration_id) {
  GeofencingRegistrations result;

  GeofencingStatus status =
      manager_->GetRegisteredRegions(service_worker_registration_id, &result);
  Send(new GeofencingMsg_GetRegisteredRegionsComplete(
      thread_id, request_id, status, result));
}

void GeofencingDispatcherHost::RegisterRegionCompleted(
    int thread_id,
    int request_id,
    GeofencingStatus status) {
  Send(new GeofencingMsg_RegisterRegionComplete(thread_id, request_id, status));
}

void GeofencingDispatcherHost::UnregisterRegionCompleted(
    int thread_id,
    int request_id,
    GeofencingStatus status) {
  Send(new GeofencingMsg_UnregisterRegionComplete(
      thread_id, request_id, status));
}

}  // namespace content
