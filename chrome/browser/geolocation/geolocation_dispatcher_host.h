// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_

#include <set>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/geolocation/location_arbitrator.h"
#include "ipc/ipc_message.h"

class GeolocationPermissionContext;
class GURL;
class ResourceMessageFilter;
class URLRequestContextGetter;
struct Geoposition;

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
      int render_view_id, int bridge_id, const std::string& host);
  void OnStartUpdating(
      int render_view_id, int bridge_id, const std::string& host,
      bool enable_high_accuracy);
  void OnStopUpdating(int render_view_id, int bridge_id);
  void OnSuspend(int render_view_id, int bridge_id);
  void OnResume(int render_view_id, int bridge_id);

  // Registers the bridge created in the renderer side. They'll delegate to the
  // UI thread if not already in there.
  void RegisterDispatcher(int process_id, int render_view_id);
  void UnregisterDispatcher(int process_id, int render_view_id);

  int resource_message_filter_process_id_;
  scoped_refptr<GeolocationPermissionContext> geolocation_permission_context_;

  struct GeolocationServiceRenderId {
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
  // Only used on the IO thread.
  std::set<GeolocationServiceRenderId> geolocation_renderers_;
  // Only set whilst we are registered with the arbitrator.
  scoped_refptr<GeolocationArbitrator> location_arbitrator_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHost);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
