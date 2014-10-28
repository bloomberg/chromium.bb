// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_

#include <map>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/geolocation/geolocation_provider_impl.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {

// GeolocationDispatcherHost is an observer for Geolocation messages.
// It's the complement of GeolocationDispatcher (owned by RenderView).
class GeolocationDispatcherHost : public WebContentsObserver {
 public:
  explicit GeolocationDispatcherHost(WebContents* web_contents);
  ~GeolocationDispatcherHost() override;

  // Pause or resumes geolocation. Resuming when nothing is paused is a no-op.
  // If the web contents is paused while not currently using geolocation but
  // then goes on to do so before being resumed, then it will not get
  // geolocation updates until it is resumed.
  void PauseOrResume(bool should_pause);

  // Enables geolocation override. This method is used by DevTools to
  // trigger possible location-specific behavior in particular web contents.
  void SetOverride(scoped_ptr<Geoposition> geoposition);

  // Disables geolocation override.
  void ClearOverride();

 private:
  // WebContentsObserver
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override;
  void RenderViewHostChanged(RenderViewHost* old_host,
                             RenderViewHost* new_host) override;
  void DidNavigateAnyFrame(RenderFrameHost* render_frame_host,
                           const LoadCommittedDetails& details,
                           const FrameNavigateParams& params) override;
  bool OnMessageReceived(const IPC::Message& msg,
                         RenderFrameHost* render_frame_host) override;

  // Message handlers:
  // TODO(mlamouri): |requesting_origin| should be a security origin to
  // guarantee that a proper origin is passed.
  void OnRequestPermission(RenderFrameHost* render_frame_host,
                           int bridge_id,
                           const GURL& requesting_origin,
                           bool user_gesture);
  void OnStartUpdating(RenderFrameHost* render_frame_host,
                       const GURL& requesting_origin,
                       bool enable_high_accuracy);
  void OnStopUpdating(RenderFrameHost* render_frame_host);

  // Updates the geolocation provider with the currently required update
  // options.
  void RefreshGeolocationOptions();

  void OnLocationUpdate(const Geoposition& position);
  void UpdateGeoposition(RenderFrameHost* frame, const Geoposition& position);

  void SendGeolocationPermissionResponse(int render_process_id,
                                         int render_frame_id,
                                         int bridge_id,
                                         bool allowed);

  // Clear pending permissions associated with a given frame and request the
  // browser to cancel the permission requests.
  void CancelPermissionRequestsForFrame(RenderFrameHost* render_frame_host);

  // A map from the RenderFrameHosts that have requested geolocation updates to
  // the type of accuracy they requested (true = high accuracy).
  std::map<RenderFrameHost*, bool> updating_frames_;
  bool paused_;

  struct PendingPermission {
    PendingPermission(int render_frame_id,
                      int render_process_id,
                      int bridge_id,
                      const GURL& origin);
    ~PendingPermission();
    int render_frame_id;
    int render_process_id;
    int bridge_id;
    GURL origin;
  };
  std::vector<PendingPermission> pending_permissions_;

  scoped_ptr<GeolocationProvider::Subscription> geolocation_subscription_;
  scoped_ptr<Geoposition> geoposition_override_;

  base::WeakPtrFactory<GeolocationDispatcherHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
