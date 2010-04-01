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
class GeolocationArbitrator;
class InfoBarDelegate;
class Profile;
class RenderViewHost;
class TabContents;

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
  void RequestGeolocationPermission(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame);

  // The render is cancelling a pending permission request.
  void CancelGeolocationPermissionRequest(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame);

  // Called once the user sets the geolocation permission.
  // It'll notify the render via ViewMsg_Geolocation_PermissionSet.
  void SetPermission(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame, bool allowed);

  // Called when a geolocation object wants to start receiving location updates.
  // Returns the location arbitrator that the caller (namely, the dispatcher
  // host) will use to receive these updates. The arbitrator is ref counted.
  // This also applies global policy around which location providers may be
  // enbaled at a given time (e.g. prior to the user agreeing to any geolocation
  // permission requests).
  GeolocationArbitrator* StartUpdatingRequested(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame);

  // Called when a geolocation object has stopped. Because we are
  // short-circuiting permission request (see StartUpdatingRequested above), we
  // cancel any pending permission in here, since WebKit doesn't know about the
  // pending permission request and will never call
  // CancelGeolocationPermissionRequest().
  void StopUpdatingRequested(
      int render_process_id, int render_view_id, int bridge_id);

  // Called by the InfoBarDelegate to notify it's closed.
  void OnInfoBarClosed(
      TabContents* tab_contents, int render_process_id, int render_view_id,
      int bridge_id);

 private:
  struct PendingInfoBarRequest;
  friend class base::RefCountedThreadSafe<GeolocationPermissionContext>;
  virtual ~GeolocationPermissionContext();

  // Triggers (or queues) the associated UI element to request permission.
  void RequestPermissionFromUI(
      TabContents* tab_contents, int render_process_id, int render_view_id,
      int bridge_id, const GURL& requesting_frame);

  // Notifies whether or not the corresponding render is allowed to use
  // geolocation.
  void NotifyPermissionSet(
      int render_process_id, int render_view_id, int bridge_id,
      const GURL& requesting_frame, bool allowed);

  // Calls GeolocationArbitrator::OnPermissionGranted.
  void NotifyArbitratorPermissionGranted(const GURL& requesting_frame);

  // Shows an infobar if there's any pending for this tab.
  void ShowQueuedInfoBar(TabContents* tab_contents, int render_process_id,
                         int render_view_id);

  void CancelPendingInfoBar(
      int render_process_id, int render_view_id, int bridge_id);

  // This should only be accessed from the UI thread.
  Profile* const profile_;

  typedef std::vector<PendingInfoBarRequest> PendingInfoBarRequests;
  PendingInfoBarRequests pending_infobar_requests_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationPermissionContext);
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_PERMISSION_CONTEXT_H_
