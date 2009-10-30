// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_

#include <set>

#include "base/basictypes.h"
#include "chrome/browser/notifications/notification.h"
#include "googleurl/src/gurl.h"

class NotificationUIManager;
class NotificationsPrefsCache;
class PrefService;
class Profile;
class Task;

// The DesktopNotificationService is an object, owned by the Profile,
// which provides the creation of desktop "toasts" to web pages and workers.
class DesktopNotificationService {
 public:
  enum NotificationSource {
    PageNotification,
    WorkerNotification
  };

  DesktopNotificationService(Profile* profile,
                             NotificationUIManager* ui_manager);
  ~DesktopNotificationService();

  // Requests permission (using an info-bar) for a given origin.
  // |callback_context| contains an opaque value to pass back to the
  // requesting process when the info-bar finishes.
  void RequestPermission(const GURL& origin, int callback_context);

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
      int process_id, int route_id, NotificationSource source,
      int notification_id);
  bool ShowDesktopNotificationText(const GURL& origin, const GURL& icon,
      const string16& title, const string16& text, int process_id,
      int route_id, NotificationSource source, int notification_id);

  // Methods to setup and modify permission preferences.
  void GrantPermission(const GURL& origin);
  void DenyPermission(const GURL& origin);

  NotificationsPrefsCache* prefs_cache() { return prefs_cache_; }

 private:
  void InitPrefs();

  // The profile which owns this object.
  Profile* profile_;

  // A cache of preferences which is accessible only on the IO thread
  // to service synchronous IPCs.
  scoped_refptr<NotificationsPrefsCache> prefs_cache_;

  // Non-owned pointer to the notification manager which manages the
  // UI for desktop toasts.
  NotificationUIManager* ui_manager_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
