// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_QUEUE_CONTROLLER_H_
#define CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_QUEUE_CONTROLLER_H_

#include "base/bind.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class GeolocationConfirmInfoBarDelegate;
class GeolocationPermissionRequestID;
class InfoBarService;
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
  typedef base::Callback<void(bool /* allowed */)> PermissionDecidedCallback;

  explicit GeolocationInfoBarQueueController(Profile* profile);
  virtual ~GeolocationInfoBarQueueController();

  // The InfoBar will be displayed immediately if the tab is not already
  // displaying one, otherwise it'll be queued.
  void CreateInfoBarRequest(const GeolocationPermissionRequestID& id,
                            const GURL& requesting_frame,
                            const GURL& embedder,
                            PermissionDecidedCallback callback);

  // Cancels a specific infobar request.
  void CancelInfoBarRequest(const GeolocationPermissionRequestID& id);

  // Called by the InfoBarDelegate to notify permission has been set.
  // It'll notify and dismiss any other pending InfoBar request for the same
  // |requesting_frame| and embedder.
  void OnPermissionSet(const GeolocationPermissionRequestID& id,
                       const GURL& requesting_frame,
                       const GURL& embedder,
                       bool update_content_setting,
                       bool allowed);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  class PendingInfoBarRequest;
  class RequestEquals;

  typedef std::vector<PendingInfoBarRequest> PendingInfoBarRequests;

  // Returns true if a geolocation infobar is already visible for the tab
  // corresponding to |id|.
  bool AlreadyShowingInfoBarForTab(
      const GeolocationPermissionRequestID& id) const;

  // Shows the next pending infobar for the tab corresponding to |id|, if any.
  // Note that this may not be the pending request whose ID is |id| if other
  // requests are higher in the queue.  If we can't show infobars because there
  // is no InfoBarService for this tab, removes all queued requests for this
  // tab.
  void ShowQueuedInfoBarForTab(const GeolocationPermissionRequestID& id);

  void ClearPendingInfoBarRequestsForTab(
      const GeolocationPermissionRequestID& id);

  void RegisterForInfoBarNotifications(InfoBarService* infobar_service);
  void UnregisterForInfoBarNotifications(InfoBarService* infobar_service);

  void UpdateContentSetting(
      const GURL& requesting_frame, const GURL& embedder, bool allowed);

  content::NotificationRegistrar registrar_;

  Profile* const profile_;
  PendingInfoBarRequests pending_infobar_requests_;
};

#endif  // CHROME_BROWSER_GEOLOCATION_GEOLOCATION_INFOBAR_QUEUE_CONTROLLER_H_
