// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_

#include <map>
#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"

class GeolocationDispatcherHost;
class GURL;
class Profile;
class RenderViewHost;

// GeolocationPermissionContext manages Geolocation permissions flow,
// creating UI elements to ask the user for permission when necessary.
// It always notifies the requesting render_view asynchronously via
// ViewMsg_Geolocation_PermissionSet.
class GeolocationPermissionContext
    : public base::RefCountedThreadSafe<GeolocationPermissionContext> {
 public:
  explicit GeolocationPermissionContext(Profile* profile);

  // The render is requesting permission to use Geolocation.
  // Response will be sent asynchronously as ViewMsg_Geolocation_PermissionSet.
  // Must be called from the IO thread.
  void RequestGeolocationPermission(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame);

  // Called once the user sets the geolocation permission.
  // It'll notify the render via ViewMsg_Geolocation_PermissionSet.
  void SetPermission(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame, bool allowed);

 private:
  friend class base::RefCountedThreadSafe<GeolocationPermissionContext>;
  virtual ~GeolocationPermissionContext();

  // Triggers the associated UI element to request permission.
  void RequestPermissionFromUI(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame);

  // Notifies whether or not the corresponding render is allowed to use
  // geolocation.
  void NotifyPermissionSet(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame, bool allowed);

  // This should only be accessed from the UI thread.
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContext);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
