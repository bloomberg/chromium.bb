// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_OLD_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_OLD_H_
#pragma once

#include "base/ref_counted.h"

class GeolocationPermissionContext;
namespace IPC { class Message; }

// GeolocationDispatcherHostOld is a delegate for Geolocation messages used by
// ResourceMessageFilter.
// It's the complement of GeolocationDispatcher (owned by RenderView).

// TODO(jknotten): Remove this class once the new client-based implementation is
// checked in (see http://crbug.com/59908).
class GeolocationDispatcherHostOld
    : public base::RefCountedThreadSafe<GeolocationDispatcherHostOld> {
 public:
  static GeolocationDispatcherHostOld* New(
      int resource_message_filter_process_id,
      GeolocationPermissionContext* geolocation_permission_context);

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled. Called in the browser process.
  virtual bool OnMessageReceived(const IPC::Message& msg, bool* msg_was_ok) = 0;

 protected:
  friend class base::RefCountedThreadSafe<GeolocationDispatcherHostOld>;
  GeolocationDispatcherHostOld() {}
  virtual ~GeolocationDispatcherHostOld() {}

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHostOld);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_OLD_H_
