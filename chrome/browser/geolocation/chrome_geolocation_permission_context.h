// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/geolocation_permission_context.h"

class GeolocationInfoBarQueueController;
class PrefService;
class Profile;

// Chrome specific implementation of GeolocationPermissionContext; manages
// Geolocation permissions flow, and delegates UI handling via
// GeolocationInfoBarQueueController.
class ChromeGeolocationPermissionContext
    : public content::GeolocationPermissionContext {
 public:
  static ChromeGeolocationPermissionContext* Create(Profile* profile);

  static void RegisterUserPrefs(PrefService *user_prefs);

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

 protected:
  explicit ChromeGeolocationPermissionContext(Profile* profile);
  virtual ~ChromeGeolocationPermissionContext();

  Profile* profile() const { return profile_; }

  // Return an instance of the infobar queue controller, creating it
  // if necessary.
  GeolocationInfoBarQueueController* QueueController();

  // Notifies whether or not the corresponding bridge is allowed to use
  // geolocation via
  // GeolocationPermissionContext::SetGeolocationPermissionResponse().
  // Called on the UI thread.
  void NotifyPermissionSet(int render_process_id,
                           int render_view_id,
                           int bridge_id,
                           const GURL& requesting_frame,
                           base::Callback<void(bool)> callback,
                           bool allowed);

  // ChromeGeolocationPermissionContext implementation:
  // Decide whether the geolocation permission should be granted.
  // Calls PermissionDecided if permission can be decided non-interactively,
  // or NotifyPermissionSet if permission decided by presenting an
  // infobar to the user. Called on the UI thread.
  virtual void DecidePermission(int render_process_id,
                                int render_view_id,
                                int bridge_id,
                                const GURL& requesting_frame,
                                const GURL& embedder,
                                base::Callback<void(bool)> callback);

  // Called when permission is granted without interactively asking
  // the user. Can be overridden to introduce additional UI flow.
  // Should ultimately ensure that NotifyPermissionSet is called.
  // Called on the UI thread.
  virtual void PermissionDecided(int render_process_id,
                                 int render_view_id,
                                 int bridge_id,
                                 const GURL& requesting_frame,
                                 const GURL& embedder,
                                 base::Callback<void(bool)> callback,
                                 bool allowed);

  // Create an InfoBarQueueController. overriden in derived classes to provide
  // additional UI flow.  Called on the UI thread.
  virtual GeolocationInfoBarQueueController* CreateQueueController();

 private:
  // Removes any pending InfoBar request.
  void CancelPendingInfoBarRequest(int render_process_id,
                                   int render_view_id,
                                   int bridge_id);

  // This must only be accessed from the UI thread.
  Profile* const profile_;

  scoped_ptr<GeolocationInfoBarQueueController>
      geolocation_infobar_queue_controller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeGeolocationPermissionContext);
};

#endif  // CHROME_BROWSER_GEOLOCATION_CHROME_GEOLOCATION_PERMISSION_CONTEXT_H_
