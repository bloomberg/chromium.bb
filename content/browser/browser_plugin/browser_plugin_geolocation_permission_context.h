// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GEOLOCATION_PERMISSION_CONTEXT_H_
#define CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GEOLOCATION_PERMISSION_CONTEXT_H_

#include "content/public/browser/geolocation_permission_context.h"

namespace content {

// Browser plugin specific implementation of GeolocationPermissionContext.
// It manages Geolocation permissions flow for BrowserPluginGuest. When a guest
// requests gelocation permission, it delegates the request to embedder though
// embedder's javascript api.
// This runs on the I/O thread. We have to return to UI thread to talk to a
// BrowserPluginGuest.
class BrowserPluginGeolocationPermissionContext :
    public GeolocationPermissionContext {
 public:
  BrowserPluginGeolocationPermissionContext();

  // GeolocationPermissionContext implementation:
  virtual void RequestGeolocationPermission(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame,
      base::Callback<void(bool)> callback) OVERRIDE;
  virtual void CancelGeolocationPermissionRequest(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame) OVERRIDE;

 private:
  virtual ~BrowserPluginGeolocationPermissionContext();

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginGeolocationPermissionContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_PLUGIN_BROWSER_PLUGIN_GEOLOCATION_PERMISSION_CONTEXT_H_
