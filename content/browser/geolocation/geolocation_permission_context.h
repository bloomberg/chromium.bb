// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
#define CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

class GeolocationInfoBarQueueController;
class GeolocationPermissionContext;
class GURL;
class InfoBarDelegate;
class Profile;
class RenderViewHost;
class TabContents;

// GeolocationPermissionContext manages Geolocation permissions flow,
// and delegates UI handling via GeolocationInfoBarQueueController.
// It always notifies the requesting render_view asynchronously via
// ViewMsg_Geolocation_PermissionSet.
class GeolocationPermissionContext
    : public base::RefCountedThreadSafe<GeolocationPermissionContext> {
 public:
  explicit GeolocationPermissionContext(Profile* profile);

  // The render is requesting permission to use Geolocation.
  // Response will be sent asynchronously as ViewMsg_Geolocation_PermissionSet.
  void RequestGeolocationPermission(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame);

  // The render is cancelling a pending permission request.
  void CancelGeolocationPermissionRequest(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame);

  // Notifies whether or not the corresponding bridge is allowed to use
  // geolocation via ViewMsg_Geolocation_PermissionSet.
  void NotifyPermissionSet(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame, bool allowed);

 private:
  friend class base::RefCountedThreadSafe<GeolocationPermissionContext>;
  virtual ~GeolocationPermissionContext();

  // Calls GeolocationArbitrator::OnPermissionGranted.
  void NotifyArbitratorPermissionGranted(const GURL& requesting_frame);

  // Removes any pending InfoBar request.
  void CancelPendingInfoBarRequest(
      int render_process_id, int render_view_id, int bridge_id);

  // This should only be accessed from the UI thread.
  Profile* const profile_;

  scoped_ptr<GeolocationInfoBarQueueController>
      geolocation_infobar_queue_controller_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContext);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
