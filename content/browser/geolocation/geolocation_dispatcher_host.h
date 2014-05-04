// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_

#include <map>
#include <set>

#include "content/browser/geolocation/geolocation_provider_impl.h"
#include "content/public/browser/browser_message_filter.h"

class GURL;

namespace content {

class GeolocationPermissionContext;

// GeolocationDispatcherHost is a browser filter for Geolocation messages.
// It's the complement of GeolocationDispatcher (owned by RenderView).
class GeolocationDispatcherHost : public BrowserMessageFilter {
 public:
  GeolocationDispatcherHost(
      int render_process_id,
      GeolocationPermissionContext* geolocation_permission_context);

  // Pause or resumes geolocation for the given |render_view_id|. Should
  // be called on the IO thread. Resuming when nothing is paused is a no-op.
  // If a renderer is paused while not currently using geolocation but
  // then goes on to do so before being resumed, then that renderer will
  // not get geolocation updates until it is resumed.
  void PauseOrResume(int render_view_id, bool should_pause);

 private:
  virtual ~GeolocationDispatcherHost();

  // GeolocationDispatcherHost
  virtual bool OnMessageReceived(const IPC::Message& msg,
                                 bool* msg_was_ok) OVERRIDE;

  // Message handlers:
  void OnRequestPermission(int render_view_id,
                           int bridge_id,
                           const GURL& requesting_frame,
                           bool user_gesture);
  void OnCancelPermissionRequest(int render_view_id,
                                 int bridge_id,
                                 const GURL& requesting_frame);
  void OnStartUpdating(int render_view_id,
                       const GURL& requesting_frame,
                       bool enable_high_accuracy);
  void OnStopUpdating(int render_view_id);

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

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
