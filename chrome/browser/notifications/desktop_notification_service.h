// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_member.h"
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
class NotificationDelegate;
class NotificationUIManager;
class PrefRegistrySyncable;
class Profile;

namespace content {
class WebContents;
struct ShowDesktopNotificationHostMsgParams;
}

namespace gfx {
class Image;
}

// The DesktopNotificationService is an object, owned by the Profile,
// which provides the creation of desktop "toasts" to web pages and workers.
class DesktopNotificationService : public ProfileKeyedService {
 public:
  enum DesktopNotificationSource {
    PageNotification,
    WorkerNotification
  };

  // Register profile-specific prefs of notifications.
  static void RegisterUserPrefs(PrefRegistrySyncable* prefs);

  DesktopNotificationService(Profile* profile,
                             NotificationUIManager* ui_manager);
  virtual ~DesktopNotificationService();

  // Requests permission (using an info-bar) for a given origin.
  // |callback_context| contains an opaque value to pass back to the
  // requesting process when the info-bar finishes.
  void RequestPermission(const GURL& origin,
                         int process_id,
                         int route_id,
                         int callback_context,
                         content::WebContents* tab);

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

  // Add a desktop notification. On non-Ash platforms this will generate a HTML
  // notification from the input parameters. On Ash it will generate a normal
  // ash notification. Returns the notification id.
  // TODO(mukai): remove these methods. HTML notifications are no longer
  // supported.
  static std::string AddNotification(const GURL& origin_url,
                                     const string16& title,
                                     const string16& message,
                                     const GURL& icon_url,
                                     const string16& replace_id,
                                     NotificationDelegate* delegate,
                                     Profile* profile);

  // Same as above, but takes a gfx::Image for the icon instead.
  static std::string AddIconNotification(const GURL& origin_url,
                                         const string16& title,
                                         const string16& message,
                                         const gfx::Image& icon,
                                         const string16& replace_id,
                                         NotificationDelegate* delegate,
                                         Profile* profile);

  // Remove any active notification corresponding to |notification_id|.
  static void RemoveNotification(const std::string& notification_id);

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

  // Returns true if the extension of the specified |id| is allowed to send
  // notifications.
  bool IsExtensionEnabled(const std::string& id);

  // Updates the availability of the extension to send notifications.
  void SetExtensionEnabled(const std::string& id, bool enabled);

 private:
  // Takes a notification object and shows it in the UI.
  void ShowNotification(const Notification& notification);

  // Returns a display name for an origin, to be used in permission infobar
  // or on the frame of the notification toast.  Different from the origin
  // itself when dealing with extensions.
  string16 DisplayNameForOrigin(const GURL& origin);

  // Notifies the observers when permissions settings change.
  void NotifySettingsChange();

  NotificationUIManager* GetUIManager();

  // Called when the disabled_extension_id pref has been changed.
  void OnDisabledExtensionIdsChanged();

  // The profile which owns this object.
  Profile* profile_;

  // Non-owned pointer to the notification manager which manages the
  // UI for desktop toasts.
  NotificationUIManager* ui_manager_;

  // Prefs listener for disabled_extension_id.
  StringListPrefMember disabled_extension_id_pref_;

  // On-memory data for the availability of extensions.
  std::set<std::string> disabled_extension_ids_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
