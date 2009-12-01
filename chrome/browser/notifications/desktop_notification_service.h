// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_

#include <set>

#include "base/basictypes.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "googleurl/src/gurl.h"

class NotificationUIManager;
class NotificationsPrefsCache;
class PrefService;
class Profile;
class Task;

// The DesktopNotificationService is an object, owned by the Profile,
// which provides the creation of desktop "toasts" to web pages and workers.
class DesktopNotificationService : public NotificationObserver {
 public:
  enum DesktopNotificationSource {
    PageNotification,
    WorkerNotification
  };

  DesktopNotificationService(Profile* profile,
                             NotificationUIManager* ui_manager);
  virtual ~DesktopNotificationService();

  // Requests permission (using an info-bar) for a given origin.
  // |callback_context| contains an opaque value to pass back to the
  // requesting process when the info-bar finishes.
  void RequestPermission(const GURL& origin,
                         int process_id,
                         int route_id,
                         int callback_context);

  // Takes a notification object and shows it in the UI.
  void ShowNotification(const Notification& notification);

  // Two ShowNotification methods: getting content either from remote
  // URL or as local parameters.  These are called on the UI thread
  // in response to IPCs from a child process running script. |origin|
  // is the origin of the script.  |source| indicates whether the
  // script is in a worker or page.  |notification_id| is an opaque
  // value to be passed back to the process when events occur on
  // this notification.
  bool ShowDesktopNotification(const GURL& origin, const GURL& url,
      int process_id, int route_id, DesktopNotificationSource source,
      int notification_id);
  bool ShowDesktopNotificationText(const GURL& origin, const GURL& icon,
      const string16& title, const string16& text, int process_id,
      int route_id, DesktopNotificationSource source, int notification_id);

  // Cancels a notification.  If it has already been shown, it will be
  // removed from the screen.  If it hasn't been shown yet, it won't be
  // shown.
  bool CancelDesktopNotification(int process_id,
                                 int route_id,
                                 int notification_id);

  // Methods to setup and modify permission preferences.
  void GrantPermission(const GURL& origin);
  void DenyPermission(const GURL& origin);

  NotificationsPrefsCache* prefs_cache() { return prefs_cache_; }

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  void InitPrefs();

  // Save a permission change to the profile.
  void PersistPermissionChange(const GURL& origin, bool is_allowed);

  // Returns a display name for an origin, to be used in permission infobar
  // or on the frame of the notification toast.  Different from the origin
  // itself when dealing with extensions.
  std::wstring DisplayNameForOrigin(const GURL& origin);

  // The profile which owns this object.
  Profile* profile_;

  // A cache of preferences which is accessible only on the IO thread
  // to service synchronous IPCs.
  scoped_refptr<NotificationsPrefsCache> prefs_cache_;

  // Non-owned pointer to the notification manager which manages the
  // UI for desktop toasts.
  NotificationUIManager* ui_manager_;

  // Connection to the service providing the other kind of notifications.
  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
