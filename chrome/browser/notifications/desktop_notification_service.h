// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/content_settings/content_settings_notification_provider.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/content_settings.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"

class Notification;
class NotificationUIManager;
class NotificationsPrefsCache;
class PrefService;
class Profile;
class TabContents;
struct DesktopNotificationHostMsg_Show_Params;

// The DesktopNotificationService is an object, owned by the Profile,
// which provides the creation of desktop "toasts" to web pages and workers.
class DesktopNotificationService : public NotificationObserver,
                                   public ProfileKeyedService {
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
  // requesting process when the info-bar finishes.  A NULL tab can be given if
  // none exist (i.e. background tab), in which case the currently selected tab
  // will be used.
  void RequestPermission(const GURL& origin,
                         int process_id,
                         int route_id,
                         int callback_context,
                         TabContents* tab);

  // ShowNotification is called on the UI thread handling IPCs from a child
  // process, identified by |process_id| and |route_id|.  |source| indicates
  // whether the script is in a worker or page. |params| contains all the
  // other parameters supplied by the worker or page.
  bool ShowDesktopNotification(
      const DesktopNotificationHostMsg_Show_Params& params,
      int process_id, int route_id, DesktopNotificationSource source);

  // Cancels a notification.  If it has already been shown, it will be
  // removed from the screen.  If it hasn't been shown yet, it won't be
  // shown.
  bool CancelDesktopNotification(int process_id,
                                 int route_id,
                                 int notification_id);

  // Methods to setup and modify permission preferences.
  void GrantPermission(const GURL& origin);
  void DenyPermission(const GURL& origin);

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  NotificationsPrefsCache* prefs_cache() { return prefs_cache_; }

  // Creates a data:xxxx URL which contains the full HTML for a notification
  // using supplied icon, title, and text, run through a template which contains
  // the standard formatting for notifications.
  static string16 CreateDataUrl(const GURL& icon_url,
                                const string16& title,
                                const string16& body,
                                WebKit::WebTextDirection dir);

  // Creates a data:xxxx URL which contains the full HTML for a notification
  // using resource template which contains the standard formatting for
  // notifications.
  static string16 CreateDataUrl(int resource,
                                const std::vector<std::string>& subst);

  // The default content setting determines how to handle origins that haven't
  // been allowed or denied yet.
  ContentSetting GetDefaultContentSetting();
  void SetDefaultContentSetting(ContentSetting setting);
  bool IsDefaultContentSettingManaged() const;

  // NOTE: This should only be called on the UI thread.
  void ResetToDefaultContentSetting();

  // Returns all origins that explicitly have been allowed.
  std::vector<GURL> GetAllowedOrigins();

  // Returns all origins that explicitly have been denied.
  std::vector<GURL> GetBlockedOrigins();

  // Removes an origin from the "explicitly allowed" set.
  void ResetAllowedOrigin(const GURL& origin);

  // Removes an origin from the "explicitly denied" set.
  void ResetBlockedOrigin(const GURL& origin);

  // Clears the sets of explicitly allowed and denied origins.
  void ResetAllOrigins();

  static void RegisterUserPrefs(PrefService* user_prefs);

  ContentSetting GetContentSetting(const GURL& origin);

 private:
  void InitPrefs();
  void StartObserving();
  void StopObserving();

  void OnPrefsChanged(const std::string& pref_name);

  // Takes a notification object and shows it in the UI.
  void ShowNotification(const Notification& notification);

  // Returns a display name for an origin, to be used in permission infobar
  // or on the frame of the notification toast.  Different from the origin
  // itself when dealing with extensions.
  string16 DisplayNameForOrigin(const GURL& origin);

  // Notifies the observers when permissions settings change.
  void NotifySettingsChange();

  // The profile which owns this object.
  Profile* profile_;

  // A cache of preferences which is accessible only on the IO thread
  // to service synchronous IPCs.
  scoped_refptr<NotificationsPrefsCache> prefs_cache_;

  // Non-owned pointer to the notification manager which manages the
  // UI for desktop toasts.
  NotificationUIManager* ui_manager_;

  scoped_ptr<content_settings::NotificationProvider> provider_;

  PrefChangeRegistrar prefs_registrar_;
  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
