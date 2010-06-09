// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_

#include "base/ref_counted.h"

class GeolocationPermissionContext;
namespace IPC { class Message; }

// GeolocationDispatcherHost is a delegate for Geolocation messages used by
// ResourceMessageFilter.
// It's the complement of GeolocationDispatcher (owned by RenderView).
class GeolocationDispatcherHost
    : public base::RefCountedThreadSafe<GeolocationDispatcherHost> {
 public:
  static GeolocationDispatcherHost* New(
      int resource_message_filter_process_id,
      GeolocationPermissionContext* geolocation_permission_context);

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled. Called in the browser process.
  virtual bool OnMessageReceived(const IPC::Message& msg, bool* msg_was_ok) = 0;

 protected:
  friend class base::RefCountedThreadSafe<GeolocationDispatcherHost>;
  GeolocationDispatcherHost() {}
  virtual ~GeolocationDispatcherHost() {}

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHost);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
