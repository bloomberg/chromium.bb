// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/platform_notification_service_impl.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/persistent_notification_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/notification_resources.h"
#include "content/public/common/platform_notification_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"
#include "ui/resources/grit/ui_resources.h"
#include "url/url_constants.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/notifier_state_tracker_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::PlatformNotificationContext;
using message_center::NotifierId;

class ProfileAttributesEntry;

namespace {

// Invalid id for a renderer process. Used in cases where we need to check for
// permission without having an associated renderer process yet.
const int kInvalidRenderProcessId = -1;

// Persistent notifications fired through the delegate do not care about the
// lifetime of the Service Worker responsible for executing the event.
void OnClickEventDispatchComplete(
    content::PersistentNotificationStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.PersistentWebNotificationClickResult", status,
      content::PersistentNotificationStatus::
          PERSISTENT_NOTIFICATION_STATUS_MAX);
}

void OnCloseEventDispatchComplete(
    content::PersistentNotificationStatus status) {
  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.PersistentWebNotificationCloseResult", status,
      content::PersistentNotificationStatus::
          PERSISTENT_NOTIFICATION_STATUS_MAX);
}

void CancelNotification(const std::string& id, ProfileID profile_id) {
  PlatformNotificationServiceImpl::GetInstance()
      ->GetNotificationUIManager()->CancelById(id, profile_id);
}

// Callback to run once the profile has been loaded in order to perform a
// given |operation| in a notification.
void ProfileLoadedCallback(
    PlatformNotificationServiceImpl::NotificationOperation operation,
    const GURL& origin,
    int64_t persistent_notification_id,
    int action_index,
    Profile* profile) {
  if (!profile) {
    // TODO(miguelg): Add UMA for this condition.
    // Perhaps propagate this through PersistentNotificationStatus.
    LOG(WARNING) << "Profile not loaded correctly";
    return;
  }

  switch (operation) {
    case PlatformNotificationServiceImpl::NOTIFICATION_CLICK:
      PlatformNotificationServiceImpl::GetInstance()
          ->OnPersistentNotificationClick(profile, persistent_notification_id,
                                          origin, action_index);
      break;
    case PlatformNotificationServiceImpl::NOTIFICATION_CLOSE:
      PlatformNotificationServiceImpl::GetInstance()
          ->OnPersistentNotificationClose(profile, persistent_notification_id,
                                          origin, true);
      break;
    case PlatformNotificationServiceImpl::NOTIFICATION_SETTINGS:
      PlatformNotificationServiceImpl::GetInstance()->OpenNotificationSettings(
          profile);
      break;
  }
}

}  // namespace

// static
PlatformNotificationServiceImpl*
PlatformNotificationServiceImpl::GetInstance() {
  return base::Singleton<PlatformNotificationServiceImpl>::get();
}

PlatformNotificationServiceImpl::PlatformNotificationServiceImpl()
    : native_notification_ui_manager_(
          NotificationUIManager::CreateNativeNotificationManager()),
      notification_ui_manager_for_tests_(nullptr) {}

PlatformNotificationServiceImpl::~PlatformNotificationServiceImpl() {}

void PlatformNotificationServiceImpl::ProcessPersistentNotificationOperation(
    NotificationOperation operation,
    const std::string& profile_id,
    bool incognito,
    const GURL& origin,
    int64_t persistent_notification_id,
    int action_index) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  DCHECK(profile_manager);

  profile_manager->LoadProfile(
      profile_id, incognito,
      base::Bind(&ProfileLoadedCallback, operation, origin,
                 persistent_notification_id, action_index));
}

void PlatformNotificationServiceImpl::OnPersistentNotificationClick(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    int action_index) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  blink::WebNotificationPermission permission =
      CheckPermissionOnUIThread(browser_context, origin,
                                kInvalidRenderProcessId);

  // TODO(peter): Change this to a CHECK() when Issue 555572 is resolved.
  // Also change this method to be const again.
  if (permission != blink::WebNotificationPermissionAllowed) {
    content::RecordAction(base::UserMetricsAction(
        "Notifications.Persistent.ClickedWithoutPermission"));
    return;
  }

  if (action_index == -1) {
    content::RecordAction(base::UserMetricsAction(
        "Notifications.Persistent.Clicked"));
  } else {
    content::RecordAction(base::UserMetricsAction(
        "Notifications.Persistent.ClickedActionButton"));
  }

  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationClickEvent(
          browser_context, persistent_notification_id, origin, action_index,
          base::Bind(&OnClickEventDispatchComplete));
}

void PlatformNotificationServiceImpl::OnPersistentNotificationClose(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    bool by_user) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If we programatically closed this notification, don't dispatch any event.
  if (closed_notifications_.erase(persistent_notification_id) != 0)
    return;

  if (by_user) {
    content::RecordAction(base::UserMetricsAction(
        "Notifications.Persistent.ClosedByUser"));
  } else {
    content::RecordAction(base::UserMetricsAction(
        "Notifications.Persistent.ClosedProgrammatically"));
  }
  content::NotificationEventDispatcher::GetInstance()
      ->DispatchNotificationCloseEvent(
          browser_context, persistent_notification_id, origin, by_user,
          base::Bind(&OnCloseEventDispatchComplete));
}

blink::WebNotificationPermission
PlatformNotificationServiceImpl::CheckPermissionOnUIThread(
    BrowserContext* browser_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

#if defined(ENABLE_EXTENSIONS)
  // Extensions support an API permission named "notification". This will grant
  // not only grant permission for using the Chrome App extension API, but also
  // for the Web Notification API.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(browser_context);
    extensions::ProcessMap* process_map =
        extensions::ProcessMap::Get(browser_context);

    const extensions::Extension* extension =
        registry->GetExtensionById(origin.host(),
                                   extensions::ExtensionRegistry::ENABLED);

    if (extension &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kNotifications) &&
        process_map->Contains(extension->id(), render_process_id)) {
      NotifierStateTracker* notifier_state_tracker =
          NotifierStateTrackerFactory::GetForProfile(profile);
      DCHECK(notifier_state_tracker);

      NotifierId notifier_id(NotifierId::APPLICATION, extension->id());
      if (notifier_state_tracker->IsNotifierEnabled(notifier_id))
        return blink::WebNotificationPermissionAllowed;
    }
  }
#endif

  ContentSetting setting =
      DesktopNotificationProfileUtil::GetContentSetting(profile, origin);

  if (setting == CONTENT_SETTING_ALLOW)
    return blink::WebNotificationPermissionAllowed;
  if (setting == CONTENT_SETTING_BLOCK)
    return blink::WebNotificationPermissionDenied;

  return blink::WebNotificationPermissionDefault;
}

blink::WebNotificationPermission
PlatformNotificationServiceImpl::CheckPermissionOnIOThread(
    content::ResourceContext* resource_context,
    const GURL& origin,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
#if defined(ENABLE_EXTENSIONS)
  // Extensions support an API permission named "notification". This will grant
  // not only grant permission for using the Chrome App extension API, but also
  // for the Web Notification API.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    extensions::InfoMap* extension_info_map = io_data->GetExtensionInfoMap();
    const extensions::ProcessMap& process_map =
        extension_info_map->process_map();

    const extensions::Extension* extension =
        extension_info_map->extensions().GetByID(origin.host());

    if (extension &&
        extension->permissions_data()->HasAPIPermission(
            extensions::APIPermission::kNotifications) &&
        process_map.Contains(extension->id(), render_process_id)) {
      if (!extension_info_map->AreNotificationsDisabled(extension->id()))
        return blink::WebNotificationPermissionAllowed;
    }
  }
#endif

  // No enabled extensions exist, so check the normal host content settings.
  HostContentSettingsMap* host_content_settings_map =
      io_data->GetHostContentSettingsMap();
  ContentSetting setting = host_content_settings_map->GetContentSetting(
      origin,
      origin,
      CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      content_settings::ResourceIdentifier());

  if (setting == CONTENT_SETTING_ALLOW)
    return blink::WebNotificationPermissionAllowed;
  if (setting == CONTENT_SETTING_BLOCK)
    return blink::WebNotificationPermissionDenied;

  return blink::WebNotificationPermissionDefault;
}

void PlatformNotificationServiceImpl::DisplayNotification(
    BrowserContext* browser_context,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources,
    std::unique_ptr<content::DesktopNotificationDelegate> delegate,
    base::Closure* cancel_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);
  DCHECK_EQ(0u, notification_data.actions.size());
  DCHECK_EQ(0u, notification_resources.action_icons.size());

  NotificationObjectProxy* proxy =
      new NotificationObjectProxy(browser_context, std::move(delegate));
  Notification notification = CreateNotificationFromData(
      profile, origin, notification_data, notification_resources, proxy);

  GetNotificationUIManager()->Add(notification, profile);
  if (cancel_callback)
    *cancel_callback =
        base::Bind(&CancelNotification,
                   notification.delegate_id(),
                   NotificationUIManager::GetProfileID(profile));

  HostContentSettingsMapFactory::GetForProfile(profile)->UpdateLastUsage(
      origin, origin, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    BrowserContext* browser_context,
    int64_t persistent_notification_id,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  // The notification settings button will be appended after the developer-
  // supplied buttons, available in |notification_data.actions|.
  int settings_button_index = notification_data.actions.size();
  PersistentNotificationDelegate* delegate = new PersistentNotificationDelegate(
      browser_context, persistent_notification_id, origin,
      settings_button_index);

  Notification notification = CreateNotificationFromData(
      profile, origin, notification_data, notification_resources, delegate);

  // TODO(peter): Remove this mapping when we have reliable id generation for
  // the message_center::Notification objects.
  persistent_notifications_[persistent_notification_id] = notification.id();

  GetNotificationUIManager()->Add(notification, profile);
  content::RecordAction(
      base::UserMetricsAction("Notifications.Persistent.Shown"));

  HostContentSettingsMapFactory::GetForProfile(profile)->UpdateLastUsage(
      origin, origin, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    BrowserContext* browser_context,
    int64_t persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  closed_notifications_.insert(persistent_notification_id);

#if defined(OS_ANDROID)
  bool cancel_by_persistent_id = true;
#else
  bool cancel_by_persistent_id = (native_notification_ui_manager_ != nullptr);
#endif

  if (cancel_by_persistent_id) {
    // TODO(peter): Remove this conversion when the notification ids are being
    // generated by the caller of this method.
    GetNotificationUIManager()->CancelById(
        base::Int64ToString(persistent_notification_id),
        NotificationUIManager::GetProfileID(profile));
  }

  auto iter = persistent_notifications_.find(persistent_notification_id);
  if (iter == persistent_notifications_.end())
    return;

  GetNotificationUIManager()->CancelById(
      iter->second, NotificationUIManager::GetProfileID(profile));

  persistent_notifications_.erase(iter);
}

bool PlatformNotificationServiceImpl::GetDisplayedPersistentNotifications(
    BrowserContext* browser_context,
    std::set<std::string>* displayed_notifications) {
  DCHECK(displayed_notifications);

#if !defined(OS_ANDROID)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!profile || profile->AsTestingProfile())
    return false;  // Tests will not have a message center.

  NotificationUIManager* ui_manager = GetNotificationUIManager();
  DCHECK(ui_manager);

  // TODO(peter): Filter for persistent notifications only.
  *displayed_notifications = ui_manager->GetAllIdsByProfile(
      NotificationUIManager::GetProfileID(profile));

  return true;
#else
  // Android cannot reliably return the notifications that are currently being
  // displayed on the platform, see the comment in NotificationUIManagerAndroid.
  return false;
#endif  // !defined(OS_ANDROID)
}

Notification PlatformNotificationServiceImpl::CreateNotificationFromData(
    Profile* profile,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources,
    NotificationDelegate* delegate) const {
  DCHECK_EQ(notification_data.actions.size(),
            notification_resources.action_icons.size());

  // TODO(peter): Handle different screen densities instead of always using the
  // 1x bitmap - crbug.com/585815.
  Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_data.title,
      notification_data.body,
      gfx::Image::CreateFrom1xBitmap(notification_resources.notification_icon),
      message_center::NotifierId(origin), base::UTF8ToUTF16(origin.host()),
      origin, notification_data.tag, message_center::RichNotificationData(),
      delegate);

  notification.set_context_message(
      DisplayNameForContextMessage(profile, origin));
  notification.set_vibration_pattern(notification_data.vibration_pattern);
  notification.set_timestamp(notification_data.timestamp);
  notification.set_renotify(notification_data.renotify);
  notification.set_silent(notification_data.silent);

  // TODO(peter): Handle different screen densities instead of always using the
  // 1x bitmap - crbug.com/585815.
  notification.set_small_image(
      gfx::Image::CreateFrom1xBitmap(notification_resources.badge));

  // Developer supplied action buttons.
  std::vector<message_center::ButtonInfo> buttons;
  for (size_t i = 0; i < notification_data.actions.size(); i++) {
    message_center::ButtonInfo button(notification_data.actions[i].title);
    // TODO(peter): Handle different screen densities instead of always using
    // the 1x bitmap - crbug.com/585815.
    button.icon =
        gfx::Image::CreateFrom1xBitmap(notification_resources.action_icons[i]);
    buttons.push_back(button);
  }
  notification.set_buttons(buttons);

  // On desktop, notifications with require_interaction==true stay on-screen
  // rather than minimizing to the notification center after a timeout.
  // On mobile, this is ignored (notifications are minimized at all times).
  if (notification_data.require_interaction)
    notification.set_never_timeout(true);

  return notification;
}

NotificationUIManager*
PlatformNotificationServiceImpl::GetNotificationUIManager() const {
  if (notification_ui_manager_for_tests_)
    return notification_ui_manager_for_tests_;

  if (native_notification_ui_manager_) {
    return native_notification_ui_manager_.get();
  }

  return g_browser_process->notification_ui_manager();
}

void PlatformNotificationServiceImpl::OpenNotificationSettings(
    BrowserContext* browser_context) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  if (switches::SettingsWindowEnabled()) {
    chrome::ShowContentSettingsExceptionsInWindow(
        profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  } else {
    chrome::ScopedTabbedBrowserDisplayer browser_displayer(profile);
    chrome::ShowContentSettingsExceptions(browser_displayer.browser(),
                                          CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  }

#endif  // defined(OS_ANDROID)
}

void PlatformNotificationServiceImpl::SetNotificationUIManagerForTesting(
    NotificationUIManager* manager) {
  notification_ui_manager_for_tests_ = manager;
}

base::string16 PlatformNotificationServiceImpl::DisplayNameForContextMessage(
    Profile* profile,
    const GURL& origin) const {
#if defined(ENABLE_EXTENSIONS)
  // If the source is an extension, lookup the display name.
  if (origin.SchemeIs(extensions::kExtensionScheme)) {
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
            origin.host(), extensions::ExtensionRegistry::EVERYTHING);
    DCHECK(extension);

    return base::UTF8ToUTF16(extension->name());
  }
#endif

  return base::string16();
}
