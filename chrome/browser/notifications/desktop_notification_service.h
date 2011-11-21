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
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextDirection.h"

class ContentSettingsPattern;
class Notification;
class NotificationUIManager;
class Profile;
class TabContents;

namespace content {
struct ShowDesktopNotificationHostMsgParams;
}

// The DesktopNotificationService is an object, owned by the Profile,
// which provides the creation of desktop "toasts" to web pages and workers.
class DesktopNotificationService : public content::NotificationObserver,
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
      const content::ShowDesktopNotificationHostMsgParams& params,
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

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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
  // been allowed or denied yet. If |provider_id| is not NULL, the id of the
  // provider which provided the default setting is assigned to it.
  ContentSetting GetDefaultContentSetting(std::string* provider_id);
  void SetDefaultContentSetting(ContentSetting setting);

  // NOTE: This should only be called on the UI thread.
  void ResetToDefaultContentSetting();

  // Returns all notifications settings. |settings| is cleared before
  // notifications setting are passed to it.
  void GetNotificationsSettings(ContentSettingsForOneType* settings);

  // Clears the notifications setting for the given pattern.
  void ClearSetting(const ContentSettingsPattern& pattern);

  // Clears the sets of explicitly allowed and denied origins.
  void ResetAllOrigins();

  ContentSetting GetContentSetting(const GURL& origin);

  // Checks to see if a given origin has permission to create desktop
  // notifications.
  WebKit::WebNotificationPresenter::Permission
      HasPermission(const GURL& origin);

 private:
  void StartObserving();
  void StopObserving();

  // Takes a notification object and shows it in the UI.
  void ShowNotification(const Notification& notification);

  // Returns a display name for an origin, to be used in permission infobar
  // or on the frame of the notification toast.  Different from the origin
  // itself when dealing with extensions.
  string16 DisplayNameForOrigin(const GURL& origin);

  // Notifies the observers when permissions settings change.
  void NotifySettingsChange();

  NotificationUIManager* GetUIManager();

  // The profile which owns this object.
  Profile* profile_;

  // Non-owned pointer to the notification manager which manages the
  // UI for desktop toasts.
  NotificationUIManager* ui_manager_;

  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
