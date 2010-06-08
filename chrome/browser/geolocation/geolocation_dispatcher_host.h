// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_

#include <map>
#include <set>
#include <utility>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/geolocation/location_arbitrator.h"

class GeolocationPermissionContext;
class GURL;
class ResourceMessageFilter;
class URLRequestContextGetter;
struct Geoposition;
namespace IPC { class Message; }

// GeolocationDispatcherHost is a delegate for Geolocation messages used by
// ResourceMessageFilter.
// It's the complement of GeolocationDispatcher (owned by RenderView).
class GeolocationDispatcherHost
    : public base::RefCountedThreadSafe<GeolocationDispatcherHost>,
      public GeolocationArbitrator::Delegate {
 public:
  GeolocationDispatcherHost(
      int resource_message_filter_process_id,
      GeolocationPermissionContext* geolocation_permission_context);

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled. Called in the browser process.
  bool OnMessageReceived(const IPC::Message& msg, bool* msg_was_ok);

  // GeolocationArbitrator::Delegate
  virtual void OnLocationUpdate(const Geoposition& position);

 private:
  friend class base::RefCountedThreadSafe<GeolocationDispatcherHost>;
  virtual ~GeolocationDispatcherHost();

  void OnRegisterDispatcher(int render_view_id);
  void OnUnregisterDispatcher(int render_view_id);
  void OnRequestPermission(
      int render_view_id, int bridge_id, const GURL& requesting_frame);
  void OnCancelPermissionRequest(
      int render_view_id, int bridge_id, const GURL& requesting_frame);
  void OnStartUpdating(
      int render_view_id, int bridge_id, const GURL& requesting_frame,
      bool enable_high_accuracy);
  void OnStopUpdating(int render_view_id, int bridge_id);
  void OnSuspend(int render_view_id, int bridge_id);
  void OnResume(int render_view_id, int bridge_id);

  // Registers the bridge created in the renderer side. They'll delegate to the
  // UI thread if not already in there.
  void RegisterDispatcher(int process_id, int render_view_id);
  void UnregisterDispatcher(int process_id, int render_view_id);
  // Updates the |location_arbitrator_| with the currently required update
  // options, based on |bridge_update_options_|.
  void RefreshUpdateOptions();

  int resource_message_filter_process_id_;
  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;

  // Iterated when sending location updates to renderer processes. The fan out
  // to individual bridge IDs happens renderer side, in order to minimize
  // context switches.
  // Only used on the IO thread.
  std::set<int> geolocation_renderer_ids_;
  // Maps <renderer_id, bridge_id> to the location arbitrator update options
  // that correspond to this particular bridge.
  typedef std::map<std::pair<int, int>, GeolocationArbitrator::UpdateOptions>
      BridgeUpdateOptionsMap;
  BridgeUpdateOptionsMap bridge_update_options_;
  // Only set whilst we are registered with the arbitrator.
  scoped_refptr<GeolocationArbitrator> location_arbitrator_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHost);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
