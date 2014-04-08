// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/geolocation_dispatcher_host.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "content/browser/geolocation/geolocation_provider_impl.h"
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

class GeolocationDispatcherHostImpl : public GeolocationDispatcherHost {
 public:
  GeolocationDispatcherHostImpl(
      int render_process_id,
      GeolocationPermissionContext* geolocation_permission_context);

  // GeolocationDispatcherHost
  virtual bool OnMessageReceived(const IPC::Message& msg,
                                 bool* msg_was_ok) OVERRIDE;

 private:
  virtual ~GeolocationDispatcherHostImpl();

  void OnRequestPermission(int render_view_id,
                           int bridge_id,
                           const GURL& requesting_frame);
  void OnCancelPermissionRequest(int render_view_id,
                                 int bridge_id,
                                 const GURL& requesting_frame);
  void OnStartUpdating(int render_view_id,
                       const GURL& requesting_frame,
                       bool enable_high_accuracy);
  void OnStopUpdating(int render_view_id);


  virtual void PauseOrResume(int render_view_id, bool should_pause) OVERRIDE;

  // Updates the |geolocation_provider_| with the currently required update
  // options.
  void RefreshGeolocationOptions();

  void OnLocationUpdate(const Geoposition& position);

  int render_process_id_;
  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;

  struct RendererGeolocationOptions {
    bool high_accuracy;
    bool is_paused;
  };

  // Used to keep track of the renderers in this process that are using
  // geolocation and the options associated with them. The map is iterated
  // when a location update is available and the fan out to individual bridge
  // IDs happens renderer side, in order to minimize context switches.
  // Only used on the IO thread.
  std::map<int, RendererGeolocationOptions> geolocation_renderers_;

  // Used by Android WebView to support that case that a renderer is in the
  // 'paused' state but not yet using geolocation. If the renderer does start
  // using geolocation while paused, we move from this set into
  // |geolocation_renderers_|. If the renderer doesn't end up wanting to use
  // geolocation while 'paused' then we remove from this set. A renderer id
  // can exist only in this set or |geolocation_renderers_|, never both.
  std::set<int> pending_paused_geolocation_renderers_;

  // Only set whilst we are registered with the geolocation provider.
  GeolocationProviderImpl* geolocation_provider_;

  GeolocationProviderImpl::LocationUpdateCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHostImpl);
};

GeolocationDispatcherHostImpl::GeolocationDispatcherHostImpl(
    int render_process_id,
    GeolocationPermissionContext* geolocation_permission_context)
    : render_process_id_(render_process_id),
      geolocation_permission_context_(geolocation_permission_context),
      geolocation_provider_(NULL) {
  callback_ = base::Bind(
      &GeolocationDispatcherHostImpl::OnLocationUpdate, base::Unretained(this));
  // This is initialized by ResourceMessageFilter. Do not add any non-trivial
  // initialization here, defer to OnRegisterBridge which is triggered whenever
  // a javascript geolocation object is actually initialized.
}

GeolocationDispatcherHostImpl::~GeolocationDispatcherHostImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (geolocation_provider_)
    geolocation_provider_->RemoveLocationUpdateCallback(callback_);
}

bool GeolocationDispatcherHostImpl::OnMessageReceived(
    const IPC::Message& msg, bool* msg_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  *msg_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(GeolocationDispatcherHostImpl, msg, *msg_was_ok)
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

void GeolocationDispatcherHostImpl::OnLocationUpdate(
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

void GeolocationDispatcherHostImpl::OnRequestPermission(
    int render_view_id,
    int bridge_id,
    const GURL& requesting_frame) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":"
           << render_view_id << ":" << bridge_id;
  if (geolocation_permission_context_.get()) {
    geolocation_permission_context_->RequestGeolocationPermission(
        render_process_id_,
        render_view_id,
        bridge_id,
        requesting_frame,
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

void GeolocationDispatcherHostImpl::OnCancelPermissionRequest(
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

void GeolocationDispatcherHostImpl::OnStartUpdating(
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

void GeolocationDispatcherHostImpl::OnStopUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DVLOG(1) << __FUNCTION__ << " " << render_process_id_ << ":"
           << render_view_id;
  DCHECK_EQ(1U, geolocation_renderers_.count(render_view_id));
  geolocation_renderers_.erase(render_view_id);
  RefreshGeolocationOptions();
}

void GeolocationDispatcherHostImpl::PauseOrResume(int render_view_id,
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

void GeolocationDispatcherHostImpl::RefreshGeolocationOptions() {
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

}  // namespace


// GeolocationDispatcherHost --------------------------------------------------

// static
GeolocationDispatcherHost* GeolocationDispatcherHost::New(
    int render_process_id,
    GeolocationPermissionContext* geolocation_permission_context) {
  return new GeolocationDispatcherHostImpl(
      render_process_id,
      geolocation_permission_context);
}

GeolocationDispatcherHost::GeolocationDispatcherHost()
    : BrowserMessageFilter(GeolocationMsgStart) {
}

GeolocationDispatcherHost::~GeolocationDispatcherHost() {
}

}  // namespace content
