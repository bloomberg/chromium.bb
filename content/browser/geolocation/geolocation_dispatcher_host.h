// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_

#include "content/public/browser/browser_message_filter.h"

namespace content {

class GeolocationPermissionContext;

// GeolocationDispatcherHost is a browser filter for Geolocation messages.
// It's the complement of GeolocationDispatcher (owned by RenderView).
class GeolocationDispatcherHost : public BrowserMessageFilter {
 public:
  static GeolocationDispatcherHost* New(
      int render_process_id,
      GeolocationPermissionContext* geolocation_permission_context);

 protected:
  GeolocationDispatcherHost();
  virtual ~GeolocationDispatcherHost();

  DISALLOW_COPY_AND_ASSIGN(GeolocationDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_DISPATCHER_HOST_H_
