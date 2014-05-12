// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_

#include "content/browser/geolocation/geolocation_provider_impl.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;

namespace content {

class GeolocationPermissionContext;

// GeolocationDispatcherHost is an observer for Geolocation messages.
// It's the complement of GeolocationDispatcher (owned by RenderView).
class GeolocationDispatcherHost : public WebContentsObserver {
 public:
  explicit GeolocationDispatcherHost(WebContents* web_contents);
  virtual ~GeolocationDispatcherHost();

  // Pause or resumes geolocation. Resuming when nothing is paused is a no-op.
  // If the web contents is paused while not currently using geolocation but
  // then goes on to do so before being resumed, then it will not get
  // geolocation updates until it is resumed.
  void PauseOrResume(bool should_pause);

 private:
  // WebContentsObserver
  virtual void RenderViewHostChanged(RenderViewHost* old_host,
                                     RenderViewHost* new_host) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  // Message handlers:
  void OnRequestPermission(int bridge_id,
                           const GURL& requesting_frame,
                           bool user_gesture);
  void OnCancelPermissionRequest(int bridge_id,
                                 const GURL& requesting_frame);
  void OnStartUpdating(const GURL& requesting_frame,
                       bool enable_high_accuracy);
  void OnStopUpdating();

  // Updates the geolocation provider with the currently required update
  // options.
  void RefreshGeolocationOptions();

  void OnLocationUpdate(const Geoposition& position);

  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;

  bool watching_requested_;
  bool paused_;
  bool high_accuracy_;

  scoped_ptr<GeolocationProvider::Subscription> geolocation_subscription_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
