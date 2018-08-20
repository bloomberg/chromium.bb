// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/platform_notification_service_impl.h"

#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/notification_resources.h"
#include "content/public/common/platform_notification_data.h"
#include "extensions/buildflags/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/message_center/public/cpp/features.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::NotificationDatabaseData;
using message_center::NotifierId;

namespace {

// Whether a web notification should be displayed when chrome is in full
// screen mode.
static bool ShouldDisplayWebNotificationOnFullScreen(Profile* profile,
                                                     const GURL& origin) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
  return false;
#endif  // defined(OS_ANDROID)

  // Check to see if this notification comes from a webpage that is displaying
  // fullscreen content.
  for (auto* browser : *BrowserList::GetInstance()) {
    // Only consider the browsers for the profile that created the notification
    if (browser->profile() != profile)
      continue;

    const content::WebContents* active_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (!active_contents)
      continue;

    // Check to see if
    //  (a) the active tab in the browser shares its origin with the
    //      notification.
    //  (b) the browser is fullscreen
    //  (c) the browser has focus.
    if (active_contents->GetURL().GetOrigin() == origin &&
        browser->exclusive_access_manager()->context()->IsFullscreen() &&
        browser->window()->IsActive()) {
      return true;
    }
  }

  return false;
}

}  // namespace

// static
PlatformNotificationServiceImpl*
PlatformNotificationServiceImpl::GetInstance() {
  return base::Singleton<PlatformNotificationServiceImpl>::get();
}

// static
void PlatformNotificationServiceImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // The first persistent ID is registered as 10000 rather than 1 to prevent the
  // reuse of persistent notification IDs, which must be unique. Reuse of
  // notification IDs may occur as they were previously stored in a different
  // data store.
  registry->RegisterIntegerPref(prefs::kNotificationNextPersistentId, 10000);
}

PlatformNotificationServiceImpl::PlatformNotificationServiceImpl() = default;

PlatformNotificationServiceImpl::~PlatformNotificationServiceImpl() = default;

bool PlatformNotificationServiceImpl::WasClosedProgrammatically(
    const std::string& notification_id) {
  return closed_notifications_.erase(notification_id) != 0;
}

// TODO(awdf): Rename to DisplayNonPersistentNotification (Similar for Close)
void PlatformNotificationServiceImpl::DisplayNotification(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Posted tasks can request notifications to be added, which would cause a
  // crash (see |ScopedKeepAlive|). We just do nothing here, the user would not
  // see the notification anyway, since we are shutting down.
  if (g_browser_process->IsShuttingDown())
    return;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);
  DCHECK_EQ(0u, notification_data.actions.size());
  DCHECK_EQ(0u, notification_resources.action_icons.size());

  message_center::Notification notification =
      CreateNotificationFromData(profile, origin, notification_id,
                                 notification_data, notification_resources);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::WEB_NON_PERSISTENT, notification);
}

void PlatformNotificationServiceImpl::DisplayPersistentNotification(
    BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& service_worker_scope,
    const GURL& origin,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Posted tasks can request notifications to be added, which would cause a
  // crash (see |ScopedKeepAlive|). We just do nothing here, the user would not
  // see the notification anyway, since we are shutting down.
  if (g_browser_process->IsShuttingDown())
    return;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  message_center::Notification notification =
      CreateNotificationFromData(profile, origin, notification_id,
                                 notification_data, notification_resources);
  auto metadata = std::make_unique<PersistentNotificationMetadata>();
  metadata->service_worker_scope = service_worker_scope;

  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::WEB_PERSISTENT, notification,
      std::move(metadata));

  NotificationMetricsLoggerFactory::GetForBrowserContext(browser_context)
      ->LogPersistentNotificationShown();
}

void PlatformNotificationServiceImpl::CloseNotification(
    BrowserContext* browser_context,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      NotificationHandler::Type::WEB_NON_PERSISTENT, notification_id);
}

void PlatformNotificationServiceImpl::ClosePersistentNotification(
    BrowserContext* browser_context,
    const std::string& notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  closed_notifications_.insert(notification_id);

  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      NotificationHandler::Type::WEB_PERSISTENT, notification_id);
}

void PlatformNotificationServiceImpl::GetDisplayedNotifications(
    BrowserContext* browser_context,
    const DisplayedNotificationsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  // Tests will not have a message center.
  if (!profile || profile->AsTestingProfile()) {
    auto displayed_notifications = std::make_unique<std::set<std::string>>();
    callback.Run(std::move(displayed_notifications),
                 false /* supports_synchronization */);
    return;
  }
  NotificationDisplayServiceFactory::GetForProfile(profile)->GetDisplayed(
      callback);
}

int64_t PlatformNotificationServiceImpl::ReadNextPersistentNotificationId(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PrefService* prefs = Profile::FromBrowserContext(browser_context)->GetPrefs();

  int64_t current_id = prefs->GetInteger(prefs::kNotificationNextPersistentId);
  int64_t next_id = current_id + 1;

  prefs->SetInteger(prefs::kNotificationNextPersistentId, next_id);
  return next_id;
}

void PlatformNotificationServiceImpl::RecordNotificationUkmEvent(
    BrowserContext* browser_context,
    const NotificationDatabaseData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Only record the event if a user explicitly interacted with the notification
  // to close it.
  if (data.closed_reason != NotificationDatabaseData::ClosedReason::USER &&
      data.num_clicks == 0 && data.num_action_button_clicks == 0) {
    return;
  }

  // Query the HistoryService so we only record a notification if the origin is
  // in the user's history.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  DCHECK(history_service);
  history_service->QueryURL(
      data.origin, /*want_visits=*/false,
      base::BindOnce(
          &PlatformNotificationServiceImpl::OnUrlHistoryQueryComplete,
          base::Unretained(this), data),
      &task_tracker_);
}

void PlatformNotificationServiceImpl::OnUrlHistoryQueryComplete(
    const content::NotificationDatabaseData& data,
    bool found_url,
    const history::URLRow& url_row,
    const history::VisitVector& visits) {
  // Post the |history_query_complete_closure_for_testing_| closure to the
  // current task runner to inform tests that the history query has completed.
  if (history_query_complete_closure_for_testing_) {
    base::PostTask(FROM_HERE,
                   std::move(history_query_complete_closure_for_testing_));
  }

  // Only record the notification if the |data.origin| is in the history
  // service.
  if (!found_url)
    return;

  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  DCHECK(recorder);
  // We are using UpdateSourceURL as notifications are not tied to a navigation.
  // This is ok from a privacy perspective as we have explicitly verified that
  // |data.origin| is in the HistoryService before recording to UKM.
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  recorder->UpdateSourceURL(source_id, data.origin);

  ukm::builders::Notification builder(source_id);

  int64_t time_until_first_click_millis =
      data.time_until_first_click_millis.has_value()
          ? data.time_until_first_click_millis.value().InMilliseconds()
          : -1;

  int64_t time_until_last_click_millis =
      data.time_until_last_click_millis.has_value()
          ? data.time_until_last_click_millis.value().InMilliseconds()
          : -1;

  int64_t time_until_close_millis =
      data.time_until_close_millis.has_value()
          ? data.time_until_close_millis.value().InMilliseconds()
          : -1;

  // TODO(yangsharon):Add did_user_open_settings field and update here.
  builder.SetClosedReason(static_cast<int>(data.closed_reason))
      .SetDidReplaceAnotherNotification(data.replaced_existing_notification)
      .SetHasBadge(!data.notification_data.badge.is_empty())
      .SetHasIcon(!data.notification_data.icon.is_empty())
      .SetHasImage(!data.notification_data.image.is_empty())
      .SetHasRenotify(data.notification_data.renotify)
      .SetHasTag(!data.notification_data.tag.empty())
      .SetIsSilent(data.notification_data.silent)
      .SetNumActions(data.notification_data.actions.size())
      .SetNumActionButtonClicks(data.num_action_button_clicks)
      .SetNumClicks(data.num_clicks)
      .SetRequireInteraction(data.notification_data.require_interaction)
      .SetTimeUntilClose(time_until_close_millis)
      .SetTimeUntilFirstClick(time_until_first_click_millis)
      .SetTimeUntilLastClick(time_until_last_click_millis)
      .Record(recorder);
}

message_center::Notification
PlatformNotificationServiceImpl::CreateNotificationFromData(
    Profile* profile,
    const GURL& origin,
    const std::string& notification_id,
    const content::PlatformNotificationData& notification_data,
    const content::NotificationResources& notification_resources) const {
  DCHECK_EQ(notification_data.actions.size(),
            notification_resources.action_icons.size());

  message_center::RichNotificationData optional_fields;

  optional_fields.settings_button_handler =
      base::FeatureList::IsEnabled(message_center::kNewStyleNotifications)
          ? message_center::SettingsButtonHandler::INLINE
          : message_center::SettingsButtonHandler::DELEGATE;

  // TODO(peter): Handle different screen densities instead of always using the
  // 1x bitmap - crbug.com/585815.
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
      notification_data.title, notification_data.body,
      gfx::Image::CreateFrom1xBitmap(notification_resources.notification_icon),
      base::UTF8ToUTF16(origin.host()), origin,
      message_center::NotifierId(origin), optional_fields,
      nullptr /* delegate */);

  notification.set_context_message(
      DisplayNameForContextMessage(profile, origin));
  notification.set_vibration_pattern(notification_data.vibration_pattern);
  notification.set_timestamp(notification_data.timestamp);
  notification.set_renotify(notification_data.renotify);
  notification.set_silent(notification_data.silent);
  if (ShouldDisplayWebNotificationOnFullScreen(profile, origin)) {
    notification.set_fullscreen_visibility(
        message_center::FullscreenVisibility::OVER_USER);
  }

  if (!notification_resources.image.drawsNothing()) {
    notification.set_type(message_center::NOTIFICATION_TYPE_IMAGE);
    notification.set_image(
        gfx::Image::CreateFrom1xBitmap(notification_resources.image));
  }

  // Badges are only supported on Android, primarily because it's the only
  // platform that makes good use of them in the status bar.
#if defined(OS_ANDROID)
  // TODO(peter): Handle different screen densities instead of always using the
  // 1x bitmap - crbug.com/585815.
  notification.set_small_image(
      gfx::Image::CreateFrom1xBitmap(notification_resources.badge));
#endif  // defined(OS_ANDROID)

  // Developer supplied action buttons.
  std::vector<message_center::ButtonInfo> buttons;
  for (size_t i = 0; i < notification_data.actions.size(); ++i) {
    const content::PlatformNotificationAction& action =
        notification_data.actions[i];
    message_center::ButtonInfo button(action.title);
    // TODO(peter): Handle different screen densities instead of always using
    // the 1x bitmap - crbug.com/585815.
    button.icon =
        gfx::Image::CreateFrom1xBitmap(notification_resources.action_icons[i]);
    if (action.type == content::PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT) {
      button.placeholder = action.placeholder.as_optional_string16().value_or(
          l10n_util::GetStringUTF16(IDS_NOTIFICATION_REPLY_PLACEHOLDER));
    }
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

base::string16 PlatformNotificationServiceImpl::DisplayNameForContextMessage(
    Profile* profile,
    const GURL& origin) const {
#if BUILDFLAG(ENABLE_EXTENSIONS)
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
