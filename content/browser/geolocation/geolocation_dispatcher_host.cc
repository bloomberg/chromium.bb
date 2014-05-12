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
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
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

void SendGeolocationPermissionResponse(int render_process_id,
                                       int render_view_id,
                                       int bridge_id,
                                       bool allowed) {
  RenderViewHostImpl* render_view_host =
      RenderViewHostImpl::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return;
  render_view_host->Send(
      new GeolocationMsg_PermissionSet(render_view_id, bridge_id, allowed));

  if (allowed)
    GeolocationProviderImpl::GetInstance()->UserDidOptIntoLocationServices();
}

}  // namespace

GeolocationDispatcherHost::GeolocationDispatcherHost(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      watching_requested_(false),
      paused_(false),
      high_accuracy_(false) {
  // This is initialized by WebContentsImpl. Do not add any non-trivial
  // initialization here, defer to OnStartUpdating which is triggered whenever
  // a javascript geolocation object is actually initialized.
}

GeolocationDispatcherHost::~GeolocationDispatcherHost() {
}

void GeolocationDispatcherHost::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {
  watching_requested_ = false;
  paused_ = false;
  geolocation_subscription_.reset();
}

bool GeolocationDispatcherHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GeolocationDispatcherHost, msg)
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordGeopositionErrorCode(geoposition.error_code);
  if (!paused_)
    Send(new GeolocationMsg_PositionUpdated(routing_id(), geoposition));
}

void GeolocationDispatcherHost::OnRequestPermission(
    int bridge_id,
    const GURL& requesting_frame,
    bool user_gesture) {
  GeolocationPermissionContext* context =
      web_contents()->GetBrowserContext()->GetGeolocationPermissionContext();
  int render_process_id = web_contents()->GetRenderProcessHost()->GetID();
  int render_view_id = web_contents()->GetRenderViewHost()->GetRoutingID();
  if (context) {
    context->RequestGeolocationPermission(
        web_contents(),
        bridge_id,
        requesting_frame,
        user_gesture,
        base::Bind(&SendGeolocationPermissionResponse,
                   render_process_id,
                   render_view_id,
                   bridge_id));
  } else {
    SendGeolocationPermissionResponse(
        render_process_id, render_view_id, bridge_id, true);
  }
}

void GeolocationDispatcherHost::OnCancelPermissionRequest(
    int bridge_id,
    const GURL& requesting_frame) {
  GeolocationPermissionContext* context =
      web_contents()->GetBrowserContext()->GetGeolocationPermissionContext();
  if (context) {
    context->CancelGeolocationPermissionRequest(
        web_contents(), bridge_id, requesting_frame);
  }
}

void GeolocationDispatcherHost::OnStartUpdating(
    const GURL& requesting_frame,
    bool enable_high_accuracy) {
  // StartUpdating() can be invoked as a result of high-accuracy mode
  // being enabled / disabled. No need to record the dispatcher again.
  UMA_HISTOGRAM_BOOLEAN(
      "Geolocation.GeolocationDispatcherHostImpl.EnableHighAccuracy",
      enable_high_accuracy);

  watching_requested_ = true;
  high_accuracy_ = enable_high_accuracy;
  RefreshGeolocationOptions();
}

void GeolocationDispatcherHost::OnStopUpdating() {
  watching_requested_ = false;
  RefreshGeolocationOptions();
}

void GeolocationDispatcherHost::PauseOrResume(bool should_pause) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  paused_ = should_pause;
  RefreshGeolocationOptions();
}

void GeolocationDispatcherHost::RefreshGeolocationOptions() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (watching_requested_ && !paused_) {
    geolocation_subscription_ = GeolocationProvider::GetInstance()->
        AddLocationUpdateCallback(
            base::Bind(&GeolocationDispatcherHost::OnLocationUpdate,
                       base::Unretained(this)),
            high_accuracy_);
  } else {
    geolocation_subscription_.reset();
  }
}

}  // namespace content
