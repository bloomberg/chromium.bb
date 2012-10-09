// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_QUEUE_CONTROLLER_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_QUEUE_CONTROLLER_H_

#include "base/bind.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class InfoBarTabHelper;
class Profile;

// This class controls the geolocation infobar queue per profile, and it's
// used by GeolocationPermissionContext.
// An alternate approach would be to have this queue per tab, and use
// notifications to broadcast when permission is set / listen to notification to
// cancel pending requests. This may be specially useful if there are other
// things listening for such notifications.
// For the time being this class is self-contained and it doesn't seem pulling
// the notification infrastructure would simplify.
class GeolocationInfoBarQueueController : content::NotificationObserver {
 public:
  typedef base::Callback<void(int /* render_process_id */,
                              int /* render_view_id */,
                              int /* bridge_id */,
                              const GURL& /* requesting_frame */,
                              base::Callback<void(bool)> /* callback */,
                              bool /* allowed */)> NotifyPermissionSetCallback;

  GeolocationInfoBarQueueController(
      NotifyPermissionSetCallback notify_permission_set_callback,
      Profile* profile);
  virtual ~GeolocationInfoBarQueueController();

  // The InfoBar will be displayed immediately if the tab is not already
  // displaying one, otherwise it'll be queued.
  void CreateInfoBarRequest(int render_process_id,
                            int render_view_id,
                            int bridge_id,
                            const GURL& requesting_frame,
                            const GURL& emebedder,
                            base::Callback<void(bool)> callback);

  // Cancels a specific infobar request.
  void CancelInfoBarRequest(int render_process_id,
                            int render_view_id,
                            int bridge_id);

  // Called by the InfoBarDelegate to notify permission has been set.
  // It'll notify and dismiss any other pending InfoBar request for the same
  // |requesting_frame| and embedder.
  void OnPermissionSet(int render_process_id,
                       int render_view_id,
                       int bridge_id,
                       const GURL& requesting_frame,
                       const GURL& embedder,
                       bool allowed);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  struct PendingInfoBarRequest;
  class RequestEquals;

  typedef std::vector<PendingInfoBarRequest> PendingInfoBarRequests;

  // Shows the first pending infobar for this tab, if any.
  void ShowQueuedInfoBar(int render_process_id, int render_view_id,
                         InfoBarTabHelper* helper);

  // Removes any pending requests for a given tab.
  void ClearPendingInfoBarRequestsForTab(int render_process_id,
                                         int render_view_id);

  InfoBarTabHelper* GetInfoBarHelper(int render_process_id, int render_view_id);
  bool AlreadyShowingInfoBar(int render_process_id, int render_view_id);

  content::NotificationRegistrar registrar_;

  NotifyPermissionSetCallback notify_permission_set_callback_;
  Profile* const profile_;
  PendingInfoBarRequests pending_infobar_requests_;
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_QUEUE_CONTROLLER_H_
