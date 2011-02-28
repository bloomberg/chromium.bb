// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#pragma once

#include "content/browser/browser_message_filter.h"

class GeolocationPermissionContext;

// GeolocationDispatcherHost is a browser filter for Geolocation messages.
// It's the complement of GeolocationDispatcher (owned by RenderView).

class GeolocationDispatcherHost : public BrowserMessageFilter {
 public:
  static GeolocationDispatcherHost* New(
      int render_process_id,
      GeolocationPermissionContext* geolocation_permission_context);

  virtual bool OnMessageReceived(const IPC::Message& msg, bool* msg_was_ok) = 0;

 protected:
  GeolocationDispatcherHost() {}
  virtual ~GeolocationDispatcherHost() {}

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHost);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
