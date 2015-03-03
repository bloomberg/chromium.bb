// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_object_proxy.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/permission_request_id.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/message_center/notifier_settings.h"

#if defined(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/notifications/notifications_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/suggest_permission_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#endif

using content::BrowserThread;
using message_center::NotifierId;

// DesktopNotificationService -------------------------------------------------

// static
void DesktopNotificationService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(
      prefs::kMessageCenterDisabledExtensionIds,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterListPref(
      prefs::kMessageCenterDisabledSystemComponentIds,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

DesktopNotificationService::DesktopNotificationService(Profile* profile)
    : PermissionContextBase(profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS),
      profile_(profile)
#if defined(ENABLE_EXTENSIONS)
      ,
      extension_registry_observer_(this)
#endif
{
  OnStringListPrefChanged(
      prefs::kMessageCenterDisabledExtensionIds, &disabled_extension_ids_);
  OnStringListPrefChanged(
      prefs::kMessageCenterDisabledSystemComponentIds,
      &disabled_system_component_ids_);
  disabled_extension_id_pref_.Init(
      prefs::kMessageCenterDisabledExtensionIds,
      profile_->GetPrefs(),
      base::Bind(
          &DesktopNotificationService::OnStringListPrefChanged,
          base::Unretained(this),
          base::Unretained(prefs::kMessageCenterDisabledExtensionIds),
          base::Unretained(&disabled_extension_ids_)));
  disabled_system_component_id_pref_.Init(
      prefs::kMessageCenterDisabledSystemComponentIds,
      profile_->GetPrefs(),
      base::Bind(
          &DesktopNotificationService::OnStringListPrefChanged,
          base::Unretained(this),
          base::Unretained(prefs::kMessageCenterDisabledSystemComponentIds),
          base::Unretained(&disabled_system_component_ids_)));
#if defined(ENABLE_EXTENSIONS)
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));
#endif
}

DesktopNotificationService::~DesktopNotificationService() {
}

void DesktopNotificationService::RequestNotificationPermission(
    content::WebContents* web_contents,
    const PermissionRequestID& request_id,
    const GURL& requesting_origin,
    bool user_gesture,
    const BrowserPermissionCallback& result_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

#if defined(ENABLE_EXTENSIONS)
  extensions::InfoMap* extension_info_map =
      extensions::ExtensionSystem::Get(profile_)->info_map();
  const extensions::Extension* extension = NULL;
  if (extension_info_map) {
    extensions::ExtensionSet extensions;
    extension_info_map->GetExtensionsWithAPIPermissionForSecurityOrigin(
        requesting_origin,
        request_id.render_process_id(),
        extensions::APIPermission::kNotifications,
        &extensions);
    for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
         iter != extensions.end(); ++iter) {
      if (IsNotifierEnabled(NotifierId(
              NotifierId::APPLICATION, (*iter)->id()))) {
        extension = iter->get();
        break;
      }
    }
  }
  if (IsExtensionWithPermissionOrSuggestInConsole(
          extensions::APIPermission::kNotifications,
          extension,
          web_contents->GetRenderViewHost())) {
    result_callback.Run(CONTENT_SETTING_ALLOW);
    return;
  }
#endif

  // Track whether the requesting and embedding origins are different when
  // permission to display Web Notifications is being requested.
  UMA_HISTOGRAM_BOOLEAN("Notifications.DifferentRequestingEmbeddingOrigins",
                        requesting_origin.GetOrigin() !=
                            web_contents->GetLastCommittedURL().GetOrigin());

  RequestPermission(web_contents,
                    request_id,
                    requesting_origin,
                    user_gesture,
                    result_callback);
}

bool DesktopNotificationService::IsNotifierEnabled(
    const NotifierId& notifier_id) const {
  switch (notifier_id.type) {
    case NotifierId::APPLICATION:
      return disabled_extension_ids_.find(notifier_id.id) ==
          disabled_extension_ids_.end();
    case NotifierId::WEB_PAGE:
      return DesktopNotificationProfileUtil::GetContentSetting(
          profile_, notifier_id.url) == CONTENT_SETTING_ALLOW;
    case NotifierId::SYSTEM_COMPONENT:
#if defined(OS_CHROMEOS)
      return disabled_system_component_ids_.find(notifier_id.id) ==
          disabled_system_component_ids_.end();
#else
      // We do not disable system component notifications.
      return true;
#endif
  }

  NOTREACHED();
  return false;
}

void DesktopNotificationService::SetNotifierEnabled(
    const NotifierId& notifier_id,
    bool enabled) {
  DCHECK_NE(NotifierId::WEB_PAGE, notifier_id.type);

  bool add_new_item = false;
  const char* pref_name = NULL;
  scoped_ptr<base::StringValue> id;
  switch (notifier_id.type) {
    case NotifierId::APPLICATION:
      pref_name = prefs::kMessageCenterDisabledExtensionIds;
      add_new_item = !enabled;
      id.reset(new base::StringValue(notifier_id.id));
      FirePermissionLevelChangedEvent(notifier_id, enabled);
      break;
    case NotifierId::SYSTEM_COMPONENT:
#if defined(OS_CHROMEOS)
      pref_name = prefs::kMessageCenterDisabledSystemComponentIds;
      add_new_item = !enabled;
      id.reset(new base::StringValue(notifier_id.id));
#else
      return;
#endif
      break;
    default:
      NOTREACHED();
  }
  DCHECK(pref_name != NULL);

  ListPrefUpdate update(profile_->GetPrefs(), pref_name);
  base::ListValue* const list = update.Get();
  if (add_new_item) {
    // AppendIfNotPresent will delete |adding_value| when the same value
    // already exists.
    list->AppendIfNotPresent(id.release());
  } else {
    list->Remove(*id, NULL);
  }
}

void DesktopNotificationService::OnStringListPrefChanged(
    const char* pref_name, std::set<std::string>* ids_field) {
  ids_field->clear();
  // Separate GetPrefs()->GetList() to analyze the crash. See crbug.com/322320
  const PrefService* pref_service = profile_->GetPrefs();
  CHECK(pref_service);
  const base::ListValue* pref_list = pref_service->GetList(pref_name);
  for (size_t i = 0; i < pref_list->GetSize(); ++i) {
    std::string element;
    if (pref_list->GetString(i, &element) && !element.empty())
      ids_field->insert(element);
    else
      LOG(WARNING) << i << "-th element is not a string for " << pref_name;
  }
}

#if defined(ENABLE_EXTENSIONS)
void DesktopNotificationService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  NotifierId notifier_id(NotifierId::APPLICATION, extension->id());
  if (IsNotifierEnabled(notifier_id))
    return;

  // The settings for ephemeral apps will be persisted across cache evictions.
  if (extensions::util::IsEphemeralApp(extension->id(), profile_))
    return;

  SetNotifierEnabled(notifier_id, true);
}
#endif

// Unlike other permission types, granting a notification for a given origin
// will not take into account the |embedder_origin|, it will only be based
// on the requesting iframe origin.
// TODO(mukai) Consider why notifications behave differently than
// other permissions. crbug.com/416894
void DesktopNotificationService::UpdateContentSetting(
    const GURL& requesting_origin,
    const GURL& embedder_origin,
    ContentSetting content_setting) {
  DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
         content_setting == CONTENT_SETTING_BLOCK);

  if (content_setting == CONTENT_SETTING_ALLOW) {
    DesktopNotificationProfileUtil::GrantPermission(
        profile_, requesting_origin);
  } else {
    DesktopNotificationProfileUtil::DenyPermission(profile_, requesting_origin);
  }
}

void DesktopNotificationService::FirePermissionLevelChangedEvent(
    const NotifierId& notifier_id, bool enabled) {
#if defined(ENABLE_EXTENSIONS)
  DCHECK_EQ(NotifierId::APPLICATION, notifier_id.type);
  extensions::api::notifications::PermissionLevel permission =
      enabled ? extensions::api::notifications::PERMISSION_LEVEL_GRANTED
              : extensions::api::notifications::PERMISSION_LEVEL_DENIED;
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(new base::StringValue(
      extensions::api::notifications::ToString(permission)));
  scoped_ptr<extensions::Event> event(new extensions::Event(
      extensions::api::notifications::OnPermissionLevelChanged::kEventName,
      args.Pass()));
  extensions::EventRouter::Get(profile_)
      ->DispatchEventToExtension(notifier_id.id, event.Pass());

  // Tell the IO thread that this extension's permission for notifications
  // has changed.
  extensions::InfoMap* extension_info_map =
      extensions::ExtensionSystem::Get(profile_)->info_map();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&extensions::InfoMap::SetNotificationsDisabled,
                 extension_info_map, notifier_id.id, !enabled));
#endif
}
