// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/content_settings/content_settings_provider.h"
#include "chrome/browser/content_settings/permission_context_base.h"
#include "chrome/browser/notifications/extension_welcome_notification.h"
#include "chrome/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"
#include "third_party/WebKit/public/platform/WebNotificationPermission.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "ui/message_center/notifier_settings.h"
#include "url/gurl.h"

class ContentSettingsPattern;
class Notification;
class NotificationDelegate;
class NotificationUIManager;
class Profile;

namespace content {
class DesktopNotificationDelegate;
class RenderFrameHost;
struct ShowDesktopNotificationHostMsgParams;
}

namespace extensions {
class ExtensionRegistry;
}

namespace gfx {
class Image;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Callback to be invoked when the result of a permission request is known.
typedef base::Callback<void(blink::WebNotificationPermission)>
    NotificationPermissionCallback;

// The DesktopNotificationService is an object, owned by the Profile,
// which provides the creation of desktop "toasts" to web pages and workers.
class DesktopNotificationService
    : public PermissionContextBase,
      public extensions::ExtensionRegistryObserver {
 public:
  // Register profile-specific prefs of notifications.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* prefs);

  DesktopNotificationService(Profile* profile,
                             NotificationUIManager* ui_manager);
  virtual ~DesktopNotificationService();

  // Requests Web Notification permission for |requesting_frame|. The |callback|
  // will be invoked after the user has made a decision.
  void RequestNotificationPermission(
      content::WebContents* web_contents,
      const PermissionRequestID& request_id,
      const GURL& requesting_frame,
      bool user_gesture,
      const NotificationPermissionCallback& callback);

  // Show a desktop notification. If |cancel_callback| is non-null, it's set to
  // a callback which can be used to cancel the notification.
  void ShowDesktopNotification(
      const content::ShowDesktopNotificationHostMsgParams& params,
      content::RenderFrameHost* render_frame_host,
      content::DesktopNotificationDelegate* delegate,
      base::Closure* cancel_callback);

  // Creates a data:xxxx URL which contains the full HTML for a notification
  // using supplied icon, title, and text, run through a template which contains
  // the standard formatting for notifications.
  static base::string16 CreateDataUrl(const GURL& icon_url,
                                      const base::string16& title,
                                      const base::string16& body,
                                      blink::WebTextDirection dir);

  // Creates a data:xxxx URL which contains the full HTML for a notification
  // using resource template which contains the standard formatting for
  // notifications.
  static base::string16 CreateDataUrl(int resource,
                                const std::vector<std::string>& subst);

  // Add a desktop notification.
  static std::string AddIconNotification(const GURL& origin_url,
                                         const base::string16& title,
                                         const base::string16& message,
                                         const gfx::Image& icon,
                                         const base::string16& replace_id,
                                         NotificationDelegate* delegate,
                                         Profile* profile);

  // Returns true if the notifier with |notifier_id| is allowed to send
  // notifications.
  bool IsNotifierEnabled(const message_center::NotifierId& notifier_id);

  // Updates the availability of the notifier.
  void SetNotifierEnabled(const message_center::NotifierId& notifier_id,
                          bool enabled);

  // Adds in a the welcome notification if required for components built
  // into Chrome that show notifications like Chrome Now.
  void ShowWelcomeNotificationIfNecessary(const Notification& notification);

 private:
  // Returns a display name for an origin in the process id, to be used in
  // permission infobar or on the frame of the notification toast.  Different
  // from the origin itself when dealing with extensions.
  base::string16 DisplayNameForOriginInProcessId(const GURL& origin,
                                                 int process_id);
  NotificationUIManager* GetUIManager();

  // Called when the string list pref has been changed.
  void OnStringListPrefChanged(
      const char* pref_name, std::set<std::string>* ids_field);

  // Called when the disabled_extension_id pref has been changed.
  void OnDisabledExtensionIdsChanged();

  // Used as a callback once a permission has been decided to convert |allowed|
  // to one of the blink::WebNotificationPermission values.
  void OnNotificationPermissionRequested(
      const base::Callback<void(blink::WebNotificationPermission)>& callback,
      bool allowed);

  void FirePermissionLevelChangedEvent(
      const message_center::NotifierId& notifier_id,
      bool enabled);

  // extensions::ExtensionRegistryObserver:
  virtual void OnExtensionUninstalled(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UninstallReason reason) OVERRIDE;

  // The profile which owns this object.
  Profile* profile_;

  // Non-owned pointer to the notification manager which manages the
  // UI for desktop toasts.
  NotificationUIManager* ui_manager_;

  // Prefs listener for disabled_extension_id.
  StringListPrefMember disabled_extension_id_pref_;

  // Prefs listener for disabled_system_component_id.
  StringListPrefMember disabled_system_component_id_pref_;

  // On-memory data for the availability of extensions.
  std::set<std::string> disabled_extension_ids_;

  // On-memory data for the availability of system_component.
  std::set<std::string> disabled_system_component_ids_;

  // An observer to listen when extension is uninstalled.
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  // Welcome Notification
  scoped_ptr<ExtensionWelcomeNotification> chrome_now_welcome_notification_;

  base::WeakPtrFactory<DesktopNotificationService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_DESKTOP_NOTIFICATION_SERVICE_H_
