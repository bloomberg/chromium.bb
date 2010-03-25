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

struct GeolocationDispatcherHost::GeolocationServiceRenderId {
  int process_id;
  int render_view_id;
  GeolocationServiceRenderId(
      int process_id, int render_view_id)
      : process_id(process_id),
        render_view_id(render_view_id) {
  }
  bool operator==(const GeolocationServiceRenderId& rhs) const {
    return process_id == rhs.process_id &&
           render_view_id == rhs.render_view_id;
  }
  bool operator<(const GeolocationServiceRenderId& rhs) const {
    if (process_id == rhs.process_id)
      return render_view_id < rhs.render_view_id;
    return process_id < rhs.process_id;
  }
};

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

  for (std::set<GeolocationServiceRenderId>::iterator geo =
       geolocation_renderers_.begin();
       geo != geolocation_renderers_.end();
       ++geo) {
    IPC::Message* message;
    if (geoposition.IsValidFix()) {
      message = new ViewMsg_Geolocation_PositionUpdated(
          geo->render_view_id, geoposition);
    } else {
      DCHECK(geoposition.IsInitialized());
      message = new ViewMsg_Geolocation_Error(
          geo->render_view_id, geoposition.error_code,
          WideToUTF8(geoposition.error_message));
    }
    CallRenderViewHost(
        geo->process_id, geo->render_view_id,
        &RenderViewHost::Send, message);
  }
}

void GeolocationDispatcherHost::OnRegisterDispatcher(int render_view_id) {
  // TODO(bulach): is this the right way to get the RendererViewHost process id
  // to be used by RenderViewHost::FromID?
  RegisterDispatcher(
      resource_message_filter_process_id_, render_view_id);
}

void GeolocationDispatcherHost::OnUnregisterDispatcher(int render_view_id) {
  UnregisterDispatcher(
      resource_message_filter_process_id_, render_view_id);
}

void GeolocationDispatcherHost::OnRequestPermission(
    int render_view_id, int bridge_id, const GURL& requesting_frame) {
  LOG(INFO) << "permission request";
  geolocation_permission_context_->RequestGeolocationPermission(
      resource_message_filter_process_id_, render_view_id, bridge_id,
      requesting_frame);
}

void GeolocationDispatcherHost::OnStartUpdating(
    int render_view_id, int bridge_id, const GURL& requesting_frame,
    bool enable_high_accuracy) {
  // WebKit sends the startupdating request before checking permissions, to
  // optimize the no-location-available case and reduce latency in the success
  // case (location lookup happens in parallel with the permission request).
  LOG(INFO) << "start updating" << render_view_id;
  if (!location_arbitrator_) {
    location_arbitrator_ =
        geolocation_permission_context_->StartUpdatingRequested(
            resource_message_filter_process_id_, render_view_id, bridge_id,
            requesting_frame);
  }
  DCHECK(location_arbitrator_);
  location_arbitrator_->AddObserver(
      this, GeolocationArbitrator::UpdateOptions(enable_high_accuracy));
}

void GeolocationDispatcherHost::OnStopUpdating(
    int render_view_id, int bridge_id) {
  LOG(INFO) << "stop updating" << render_view_id;
  if (location_arbitrator_)
    location_arbitrator_->RemoveObserver(this);
  location_arbitrator_ = NULL;
}

void GeolocationDispatcherHost::OnSuspend(
    int render_view_id, int bridge_id) {
  LOG(INFO) << "suspend" << render_view_id;
  // TODO(bulach): connect this with GeolocationArbitrator.
}

void GeolocationDispatcherHost::OnResume(
    int render_view_id, int bridge_id) {
  LOG(INFO) << "resume" << render_view_id;
  // TODO(bulach): connect this with GeolocationArbitrator.
}

void GeolocationDispatcherHost::RegisterDispatcher(
    int process_id, int render_view_id) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this, &GeolocationDispatcherHost::RegisterDispatcher,
            process_id, render_view_id));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  GeolocationServiceRenderId geolocation_render_id(
      process_id, render_view_id);
  std::set<GeolocationServiceRenderId>::const_iterator existing =
      geolocation_renderers_.find(geolocation_render_id);
  DCHECK(existing == geolocation_renderers_.end());
  geolocation_renderers_.insert(geolocation_render_id);
}

void GeolocationDispatcherHost::UnregisterDispatcher(
    int process_id, int render_view_id) {
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(
            this, &GeolocationDispatcherHost::UnregisterDispatcher,
            process_id, render_view_id));
    return;
  }
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  GeolocationServiceRenderId geolocation_render_id(
      process_id, render_view_id);
  std::set<GeolocationServiceRenderId>::iterator existing =
      geolocation_renderers_.find(geolocation_render_id);
  DCHECK(existing != geolocation_renderers_.end());
  geolocation_renderers_.erase(existing);
}
