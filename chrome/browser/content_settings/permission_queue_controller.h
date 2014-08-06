// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_QUEUE_CONTROLLER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_QUEUE_CONTROLLER_H_

#include "base/bind.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;
class PermissionRequestID;
class InfoBarService;
class Profile;

// This class controls an infobar queue per profile, and it's used by
// GeolocationPermissionContext, and so on.
// An alternate approach would be to have this queue per tab, and use
// notifications to broadcast when permission is set / listen to notification to
// cancel pending requests. This may be specially useful if there are other
// things listening for such notifications.
// For the time being this class is self-contained and it doesn't seem pulling
// the notification infrastructure would simplify.
class PermissionQueueController : public content::NotificationObserver {
 public:
  typedef base::Callback<void(bool /* allowed */)> PermissionDecidedCallback;

  PermissionQueueController(Profile* profile, ContentSettingsType type);
  virtual ~PermissionQueueController();

  // The InfoBar will be displayed immediately if the tab is not already
  // displaying one, otherwise it'll be queued.
  void CreateInfoBarRequest(const PermissionRequestID& id,
                            const GURL& requesting_frame,
                            const GURL& embedder,
                            PermissionDecidedCallback callback);

  // Cancels a specific infobar request.
  void CancelInfoBarRequest(const PermissionRequestID& id);

  // Called by the InfoBarDelegate to notify permission has been set.
  // It'll notify and dismiss any other pending InfoBar request for the same
  // |requesting_frame| and embedder.
  void OnPermissionSet(const PermissionRequestID& id,
                       const GURL& requesting_frame,
                       const GURL& embedder,
                       bool update_content_setting,
                       bool allowed);

  // Performs the update to content settings for a particular request frame
  // context.
  void UpdateContentSetting(
      const GURL& requesting_frame, const GURL& embedder, bool allowed);

 protected:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  class PendingInfobarRequest;
  class RequestEquals;

  typedef std::vector<PendingInfobarRequest> PendingInfobarRequests;

  // Returns true if a geolocation infobar is already visible for the tab
  // corresponding to |id|.
  bool AlreadyShowingInfoBarForTab(const PermissionRequestID& id) const;

  // Shows the next pending infobar for the tab corresponding to |id|, if any.
  // Note that this may not be the pending request whose ID is |id| if other
  // requests are higher in the queue.  If we can't show infobars because there
  // is no InfoBarService for this tab, removes all queued requests for this
  // tab.
  void ShowQueuedInfoBarForTab(const PermissionRequestID& id);

  void ClearPendingInfobarRequestsForTab(const PermissionRequestID& id);

  void RegisterForInfoBarNotifications(InfoBarService* infobar_service);
  void UnregisterForInfoBarNotifications(InfoBarService* infobar_service);

  content::NotificationRegistrar registrar_;

  Profile* const profile_;
  ContentSettingsType type_;
  PendingInfobarRequests pending_infobar_requests_;
  bool in_shutdown_;

  DISALLOW_COPY_AND_ASSIGN(PermissionQueueController);
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_PERMISSION_QUEUE_CONTROLLER_H_
