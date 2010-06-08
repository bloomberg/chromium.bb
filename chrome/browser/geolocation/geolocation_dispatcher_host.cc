// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geolocation/geolocation_dispatcher_host.h"

#include "chrome/common/geoposition.h"
#include "chrome/browser/geolocation/geolocation_permission_context.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/browser/renderer_host/resource_message_filter.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_message.h"

GeolocationDispatcherHost::GeolocationDispatcherHost(
    int resource_message_filter_process_id,
    GeolocationPermissionContext* geolocation_permission_context)
    : resource_message_filter_process_id_(resource_message_filter_process_id),
      geolocation_permission_context_(geolocation_permission_context) {
  // This is initialized by ResourceMessageFilter. Do not add any non-trivial
  // initialization here, defer to OnRegisterBridge which is triggered whenever
  // a javascript geolocation object is actually initialized.
}

GeolocationDispatcherHost::~GeolocationDispatcherHost() {
  if (location_arbitrator_)
    location_arbitrator_->RemoveObserver(this);
}

bool GeolocationDispatcherHost::OnMessageReceived(
    const IPC::Message& msg, bool* msg_was_ok) {
  *msg_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GeolocationDispatcherHost, msg, *msg_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Geolocation_RegisterDispatcher,
                        OnRegisterDispatcher)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Geolocation_UnregisterDispatcher,
                        OnUnregisterDispatcher)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Geolocation_CancelPermissionRequest,
                        OnCancelPermissionRequest)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Geolocation_RequestPermission,
                        OnRequestPermission)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Geolocation_StartUpdating,
                        OnStartUpdating)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Geolocation_StopUpdating,
                        OnStopUpdating)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Geolocation_Suspend,
                        OnSuspend)
    IPC_MESSAGE_HANDLER(ViewHostMsg_Geolocation_Resume,
                        OnResume)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GeolocationDispatcherHost::OnLocationUpdate(
    const Geoposition& geoposition) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  for (std::set<int>::iterator it = geolocation_renderer_ids_.begin();
       it != geolocation_renderer_ids_.end(); ++it) {
    IPC::Message* message =
        new ViewMsg_Geolocation_PositionUpdated(*it, geoposition);
    CallRenderViewHost(resource_message_filter_process_id_, *it,
                       &RenderViewHost::Send, message);
  }
}

void GeolocationDispatcherHost::OnRegisterDispatcher(int render_view_id) {
  RegisterDispatcher(
      resource_message_filter_process_id_, render_view_id);
}

void GeolocationDispatcherHost::OnUnregisterDispatcher(int render_view_id) {
  UnregisterDispatcher(
      resource_message_filter_process_id_, render_view_id);
}

void GeolocationDispatcherHost::OnRequestPermission(
    int render_view_id, int bridge_id, const GURL& requesting_frame) {
  DLOG(INFO) << __FUNCTION__ << " " << resource_message_filter_process_id_
             << ":" << render_view_id << ":" << bridge_id;
  geolocation_permission_context_->RequestGeolocationPermission(
      resource_message_filter_process_id_, render_view_id, bridge_id,
      requesting_frame);
}

void GeolocationDispatcherHost::OnCancelPermissionRequest(
    int render_view_id, int bridge_id, const GURL& requesting_frame) {
  DLOG(INFO) << __FUNCTION__ << " " << resource_message_filter_process_id_
             << ":" << render_view_id << ":" << bridge_id;
  geolocation_permission_context_->CancelGeolocationPermissionRequest(
      resource_message_filter_process_id_, render_view_id, bridge_id,
      requesting_frame);
}

void GeolocationDispatcherHost::OnStartUpdating(
    int render_view_id, int bridge_id, const GURL& requesting_frame,
    bool enable_high_accuracy) {
  // WebKit sends the startupdating request before checking permissions, to
  // optimize the no-location-available case and reduce latency in the success
  // case (location lookup happens in parallel with the permission request).
  DLOG(INFO) << __FUNCTION__ << " " << resource_message_filter_process_id_
             << ":" << render_view_id << ":" << bridge_id;
  bridge_update_options_[std::make_pair(render_view_id, bridge_id)] =
      GeolocationArbitrator::UpdateOptions(enable_high_accuracy);
  location_arbitrator_ =
      geolocation_permission_context_->StartUpdatingRequested(
          resource_message_filter_process_id_, render_view_id, bridge_id,
          requesting_frame);
  RefreshUpdateOptions();
}

void GeolocationDispatcherHost::OnStopUpdating(
    int render_view_id, int bridge_id) {
  DLOG(INFO) << __FUNCTION__ << " " << resource_message_filter_process_id_
             << ":" << render_view_id << ":" << bridge_id;
  if (bridge_update_options_.erase(std::make_pair(render_view_id, bridge_id)))
    RefreshUpdateOptions();
  geolocation_permission_context_->StopUpdatingRequested(
      resource_message_filter_process_id_, render_view_id, bridge_id);
}

void GeolocationDispatcherHost::OnSuspend(
    int render_view_id, int bridge_id) {
  DLOG(INFO) << __FUNCTION__ << " " << resource_message_filter_process_id_
             << ":" << render_view_id << ":" << bridge_id;
  // TODO(bulach): connect this with GeolocationArbitrator.
}

void GeolocationDispatcherHost::OnResume(
    int render_view_id, int bridge_id) {
  DLOG(INFO) << __FUNCTION__ << " " << resource_message_filter_process_id_
             << ":" << render_view_id << ":" << bridge_id;
  // TODO(bulach): connect this with GeolocationArbitrator.
}

void GeolocationDispatcherHost::RegisterDispatcher(
    int process_id, int render_view_id) {
  DCHECK_EQ(resource_message_filter_process_id_, process_id);
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this, &GeolocationDispatcherHost::RegisterDispatcher,
            process_id, render_view_id));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  DCHECK_EQ(0u, geolocation_renderer_ids_.count(render_view_id));
  geolocation_renderer_ids_.insert(render_view_id);
}

void GeolocationDispatcherHost::UnregisterDispatcher(
    int process_id, int render_view_id) {
  DCHECK_EQ(resource_message_filter_process_id_, process_id);
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this, &GeolocationDispatcherHost::UnregisterDispatcher,
            process_id, render_view_id));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  DCHECK_EQ(1u, geolocation_renderer_ids_.count(render_view_id));
  geolocation_renderer_ids_.erase(render_view_id);
}

void GeolocationDispatcherHost::RefreshUpdateOptions() {
  DCHECK(location_arbitrator_);
  if (bridge_update_options_.empty()) {
    location_arbitrator_->RemoveObserver(this);
    location_arbitrator_ = NULL;
  } else {
    location_arbitrator_->AddObserver(
        this,
        GeolocationArbitrator::UpdateOptions::Collapse(
            bridge_update_options_));
  }
}
