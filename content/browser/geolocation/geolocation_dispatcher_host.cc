// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_dispatcher_host.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/geolocation_permission_context.h"
#include "content/public/common/geoposition.h"
#include "content/common/geolocation_messages.h"

namespace content {
namespace {

// Geoposition error codes for reporting in UMA.
enum GeopositionErrorCode {
  // NOTE: Do not renumber these as that would confuse interpretation of
  // previously logged data. When making changes, also update the enum list
  // in tools/metrics/histograms/histograms.xml to keep it in sync.

  // There was no error.
  GEOPOSITION_ERROR_CODE_NONE = 0,

  // User denied use of geolocation.
  GEOPOSITION_ERROR_CODE_PERMISSION_DENIED = 1,

  // Geoposition could not be determined.
  GEOPOSITION_ERROR_CODE_POSITION_UNAVAILABLE = 2,

  // Timeout.
  GEOPOSITION_ERROR_CODE_TIMEOUT = 3,

  // NOTE: Add entries only immediately above this line.
  GEOPOSITION_ERROR_CODE_COUNT = 4
};

void RecordGeopositionErrorCode(Geoposition::ErrorCode error_code) {
  GeopositionErrorCode code = GEOPOSITION_ERROR_CODE_NONE;
  switch (error_code) {
    case Geoposition::ERROR_CODE_NONE:
      code = GEOPOSITION_ERROR_CODE_NONE;
      break;
    case Geoposition::ERROR_CODE_PERMISSION_DENIED:
      code = GEOPOSITION_ERROR_CODE_PERMISSION_DENIED;
      break;
    case Geoposition::ERROR_CODE_POSITION_UNAVAILABLE:
      code = GEOPOSITION_ERROR_CODE_POSITION_UNAVAILABLE;
      break;
    case Geoposition::ERROR_CODE_TIMEOUT:
      code = GEOPOSITION_ERROR_CODE_TIMEOUT;
      break;
  }
  UMA_HISTOGRAM_ENUMERATION("Geolocation.LocationUpdate.ErrorCode",
                            code,
                            GEOPOSITION_ERROR_CODE_COUNT);
}

void NotifyGeolocationProviderPermissionGranted() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  GeolocationProviderImpl::GetInstance()->UserDidOptIntoLocationServices();
}

void SendGeolocationPermissionResponse(int render_process_id,
                                       int render_view_id,
                                       int bridge_id,
                                       bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderViewHostImpl* render_view_host =
      RenderViewHostImpl::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return;
  render_view_host->Send(
      new GeolocationMsg_PermissionSet(render_view_id, bridge_id, allowed));

  if (allowed) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&NotifyGeolocationProviderPermissionGranted));
  }
}

}  // namespace

GeolocationDispatcherHost::GeolocationDispatcherHost(
    int render_process_id,
    GeolocationPermissionContext* geolocation_permission_context)
    : BrowserMessageFilter(GeolocationMsgStart),
      render_process_id_(render_process_id),
      geolocation_permission_context_(geolocation_permission_context),
      geolocation_provider_(NULL) {
  callback_ = base::Bind(
      &GeolocationDispatcherHost::OnLocationUpdate, base::Unretained(this));
  // This is initialized by ResourceMessageFilter. Do not add any non-trivial
  // initialization here, defer to OnRegisterBridge which is triggered whenever
  // a javascript geolocation object is actually initialized.
}

GeolocationDispatcherHost::~GeolocationDispatcherHost() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (geolocation_provider_)
    geolocation_provider_->RemoveLocationUpdateCallback(callback_);
}

bool GeolocationDispatcherHost::OnMessageReceived(
    const IPC::Message& msg, bool* msg_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  *msg_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GeolocationDispatcherHost, msg, *msg_was_ok)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_CancelPermissionRequest,
                        OnCancelPermissionRequest)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_RequestPermission,
                        OnRequestPermission)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_StartUpdating, OnStartUpdating)
    IPC_MESSAGE_HANDLER(GeolocationHostMsg_StopUpdating, OnStopUpdating)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void GeolocationDispatcherHost::OnLocationUpdate(
    const Geoposition& geoposition) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  RecordGeopositionErrorCode(geoposition.error_code);
  for (std::map<int, RendererGeolocationOptions>::iterator it =
       geolocation_renderers_.begin();
       it != geolocation_renderers_.end(); ++it) {
    if (!(it->second.is_paused))
      Send(new GeolocationMsg_PositionUpdated(it->first, geoposition));
  }
}

void GeolocationDispatcherHost::OnRequestPermission(
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":"
           << render_view_id << ":" << bridge_id;
  if (geolocation_permission_context_.get()) {
    geolocation_permission_context_->RequestGeolocationPermission(
        render_process_id_,
        render_view_id,
        bridge_id,
        requesting_frame,
        user_gesture,
        base::Bind(&SendGeolocationPermissionResponse,
                   render_process_id_,
                   render_view_id,
                   bridge_id));
  } else {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&SendGeolocationPermissionResponse, render_process_id_,
                   render_view_id, bridge_id, true));
  }
}

void GeolocationDispatcherHost::OnCancelPermissionRequest(
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":"
           << render_view_id << ":" << bridge_id;
  if (geolocation_permission_context_.get()) {
    geolocation_permission_context_->CancelGeolocationPermissionRequest(
        render_process_id_, render_view_id, bridge_id, requesting_frame);
  }
}

void GeolocationDispatcherHost::OnStartUpdating(
    int render_view_id,
    const GURL& requesting_frame,
    bool enable_high_accuracy) {
  // StartUpdating() can be invoked as a result of high-accuracy mode
  // being enabled / disabled. No need to record the dispatcher again.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":"
           << render_view_id;
  UMA_HISTOGRAM_BOOLEAN(
      "Geolocation.GeolocationDispatcherHostImpl.EnableHighAccuracy",
      enable_high_accuracy);

  std::map<int, RendererGeolocationOptions>::iterator it =
            geolocation_renderers_.find(render_view_id);
  if (it == geolocation_renderers_.end()) {
    bool should_start_paused = false;
    if (pending_paused_geolocation_renderers_.erase(render_view_id) == 1) {
      should_start_paused = true;
    }
    RendererGeolocationOptions opts = {
      enable_high_accuracy,
      should_start_paused
    };
    geolocation_renderers_[render_view_id] = opts;
  } else {
    it->second.high_accuracy = enable_high_accuracy;
  }
  RefreshGeolocationOptions();
}

void GeolocationDispatcherHost::OnStopUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":"
           << render_view_id;
  DCHECK_EQ(1U, geolocation_renderers_.count(render_view_id));
  geolocation_renderers_.erase(render_view_id);
  RefreshGeolocationOptions();
}

void GeolocationDispatcherHost::PauseOrResume(int render_view_id,
                                              bool should_pause) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  std::map<int, RendererGeolocationOptions>::iterator it =
      geolocation_renderers_.find(render_view_id);
  if (it == geolocation_renderers_.end()) {
    // This renderer is not using geolocation yet, but if it does before
    // we get a call to resume, we should start it up in the paused state.
    if (should_pause) {
      pending_paused_geolocation_renderers_.insert(render_view_id);
    } else {
      pending_paused_geolocation_renderers_.erase(render_view_id);
    }
  } else {
    RendererGeolocationOptions* opts = &(it->second);
    if (opts->is_paused != should_pause)
      opts->is_paused = should_pause;
    RefreshGeolocationOptions();
  }
}

void GeolocationDispatcherHost::RefreshGeolocationOptions() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  bool needs_updates = false;
  bool use_high_accuracy = false;
  std::map<int, RendererGeolocationOptions>::const_iterator i =
      geolocation_renderers_.begin();
  for (; i != geolocation_renderers_.end(); ++i) {
    needs_updates |= !(i->second.is_paused);
    use_high_accuracy |= i->second.high_accuracy;
    if (needs_updates && use_high_accuracy)
      break;
  }
  if (needs_updates) {
    if (!geolocation_provider_)
      geolocation_provider_ = GeolocationProviderImpl::GetInstance();
    // Re-add to re-establish our options, in case they changed.
    geolocation_provider_->AddLocationUpdateCallback(
        callback_, use_high_accuracy);
  } else {
    if (geolocation_provider_)
      geolocation_provider_->RemoveLocationUpdateCallback(callback_);
    geolocation_provider_ = NULL;
  }
}

}  // namespace content
