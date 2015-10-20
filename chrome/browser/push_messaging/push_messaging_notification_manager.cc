// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"

#include <bitset>

#include "base/metrics/histogram_macros.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/rappor/rappor_utils.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/platform_notification_context.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using content::BrowserThread;

namespace {

void RecordUserVisibleStatus(content::PushUserVisibleStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.UserVisibleStatus",
                            status,
                            content::PUSH_USER_VISIBLE_STATUS_LAST + 1);
}

}  // namespace

PushMessagingNotificationManager::PushMessagingNotificationManager(
    Profile* profile)
    : profile_(profile),
      weak_factory_(this) {}

PushMessagingNotificationManager::~PushMessagingNotificationManager() {}

void PushMessagingNotificationManager::EnforceUserVisibleOnlyRequirements(
    const GURL& requesting_origin, int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(johnme): Relax this heuristic slightly.
  scoped_refptr<content::PlatformNotificationContext> notification_context =
      content::BrowserContext::GetStoragePartitionForSite(
          profile_, requesting_origin)->GetPlatformNotificationContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &content::PlatformNotificationContext
                  ::ReadAllNotificationDataForServiceWorkerRegistration,
          notification_context,
          requesting_origin, service_worker_registration_id,
          base::Bind(
              &PushMessagingNotificationManager
                  ::DidGetNotificationsFromDatabaseIOProxy,
              weak_factory_.GetWeakPtr(),
              requesting_origin, service_worker_registration_id,
              message_handled_closure)));
}

// static
void PushMessagingNotificationManager::DidGetNotificationsFromDatabaseIOProxy(
    const base::WeakPtr<PushMessagingNotificationManager>& ui_weak_ptr,
    const GURL& requesting_origin,
    int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure,
    bool success,
    const std::vector<content::NotificationDatabaseData>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PushMessagingNotificationManager
                     ::DidGetNotificationsFromDatabase,
                 ui_weak_ptr,
                 requesting_origin, service_worker_registration_id,
                 message_handled_closure,
                 success, data));
}

void PushMessagingNotificationManager::DidGetNotificationsFromDatabase(
    const GURL& requesting_origin, int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure,
    bool success, const std::vector<content::NotificationDatabaseData>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(johnme): Hiding an existing notification should also count as a useful
  // user-visible action done in response to a push message - but make sure that
  // sending two messages in rapid succession which show then hide a
  // notification doesn't count.
  int notification_count = success ? data.size() : 0;
  bool notification_shown = notification_count > 0;

  bool notification_needed = true;
  // Sites with a currently visible tab don't need to show notifications.
#if defined(OS_ANDROID)
  for (auto it = TabModelList::begin(); it != TabModelList::end(); ++it) {
    Profile* profile = (*it)->GetProfile();
    content::WebContents* active_web_contents =
        (*it)->GetActiveWebContents();
#else
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    Profile* profile = it->profile();
    content::WebContents* active_web_contents =
        it->tab_strip_model()->GetActiveWebContents();
#endif
    if (!active_web_contents || !active_web_contents->GetMainFrame())
      continue;

    // Don't leak information from other profiles.
    if (profile != profile_)
      continue;

    // Ignore minimized windows etc.
    switch (active_web_contents->GetMainFrame()->GetVisibilityState()) {
     case blink::WebPageVisibilityStateHidden:
     case blink::WebPageVisibilityStatePrerender:
      continue;
     case blink::WebPageVisibilityStateVisible:
      break;
    }

    // Use the visible URL since that's the one the user is aware of (and it
    // doesn't matter whether the page loaded successfully).
    const GURL& active_url = active_web_contents->GetVisibleURL();
    if (requesting_origin == active_url.GetOrigin()) {
      notification_needed = false;
      break;
    }
#if defined(OS_ANDROID)
  }
#else
  }
#endif

  // If more than two notifications are showing for this Service Worker, close
  // the default notification if it happens to be part of this group.
  if (notification_count >= 2) {
    for (const auto& notification_database_data : data) {
      if (notification_database_data.notification_data.tag !=
          kPushMessagingForcedNotificationTag)
        continue;

      PlatformNotificationServiceImpl* platform_notification_service =
          PlatformNotificationServiceImpl::GetInstance();

      // First close the notification for the user's point of view, and then
      // manually tell the service that the notification has been closed in
      // order to avoid duplicating the thread-jump logic.
      platform_notification_service->ClosePersistentNotification(
          profile_, notification_database_data.notification_id);
      platform_notification_service->OnPersistentNotificationClose(
          profile_, notification_database_data.notification_id,
          notification_database_data.origin, false /* by_user */);

      break;
    }
  }

  // Don't track push messages that didn't show a notification but were exempt
  // from needing to do so.
  if (notification_shown || notification_needed) {
    content::ServiceWorkerContext* service_worker_context =
        content::BrowserContext::GetStoragePartitionForSite(
            profile_, requesting_origin)->GetServiceWorkerContext();

    content::PushMessagingService::GetNotificationsShownByLastFewPushes(
        service_worker_context, service_worker_registration_id,
        base::Bind(&PushMessagingNotificationManager
                       ::DidGetNotificationsShownAndNeeded,
                   weak_factory_.GetWeakPtr(),
                   requesting_origin, service_worker_registration_id,
                   notification_shown, notification_needed,
                   message_handled_closure));
  } else {
    RecordUserVisibleStatus(
        content::PUSH_USER_VISIBLE_STATUS_NOT_REQUIRED_AND_NOT_SHOWN);
    message_handled_closure.Run();
  }
}

static void IgnoreResult(bool unused) {
}

void PushMessagingNotificationManager::DidGetNotificationsShownAndNeeded(
    const GURL& requesting_origin, int64_t service_worker_registration_id,
    bool notification_shown, bool notification_needed,
    const base::Closure& message_handled_closure,
    const std::string& data, bool success, bool not_found) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::ServiceWorkerContext* service_worker_context =
      content::BrowserContext::GetStoragePartitionForSite(
          profile_, requesting_origin)->GetServiceWorkerContext();

  // We remember whether the last (up to) 10 pushes showed notifications.
  const size_t MISSED_NOTIFICATIONS_LENGTH = 10;
  // data is a string like "0001000", where '0' means shown, and '1' means
  // needed but not shown. We manipulate it in bitset form.
  std::bitset<MISSED_NOTIFICATIONS_LENGTH> missed_notifications(data);

  DCHECK(notification_shown || notification_needed);  // Caller must ensure this
  bool needed_but_not_shown = notification_needed && !notification_shown;

  // New entries go at the end, and old ones are shifted off the beginning once
  // the history length is exceeded.
  missed_notifications <<= 1;
  missed_notifications[0] = needed_but_not_shown;
  std::string updated_data(missed_notifications.
      to_string<char, std::string::traits_type, std::string::allocator_type>());
  content::PushMessagingService::SetNotificationsShownByLastFewPushes(
      service_worker_context, service_worker_registration_id,
      requesting_origin, updated_data,
      base::Bind(&IgnoreResult));  // This is a heuristic; ignore failure.

  if (notification_shown) {
    RecordUserVisibleStatus(
        notification_needed
        ? content::PUSH_USER_VISIBLE_STATUS_REQUIRED_AND_SHOWN
        : content::PUSH_USER_VISIBLE_STATUS_NOT_REQUIRED_BUT_SHOWN);
    message_handled_closure.Run();
    return;
  }
  DCHECK(needed_but_not_shown);
  if (missed_notifications.count() <= 1) {  // Apply grace.
    RecordUserVisibleStatus(
      content::PUSH_USER_VISIBLE_STATUS_REQUIRED_BUT_NOT_SHOWN_USED_GRACE);
    message_handled_closure.Run();
    return;
  }
  RecordUserVisibleStatus(
      content::
          PUSH_USER_VISIBLE_STATUS_REQUIRED_BUT_NOT_SHOWN_GRACE_EXCEEDED);
  rappor::SampleDomainAndRegistryFromGURL(
      g_browser_process->rappor_service(),
      "PushMessaging.GenericNotificationShown.Origin",
      requesting_origin);

  // The site failed to show a notification when one was needed, and they have
  // already failed once in the previous 10 push messages, so we will show a
  // generic notification. See https://crbug.com/437277.
  //
  // TODO(johnme): The generic notification should probably automatically close
  // itself when the next push message arrives?
  content::PlatformNotificationData notification_data;
  notification_data.title =
      url_formatter::FormatUrlForSecurityDisplayOmitScheme(
          requesting_origin,
          profile_->GetPrefs()->GetString(prefs::kAcceptLanguages));
  notification_data.direction =
      content::PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT;
  notification_data.body =
      l10n_util::GetStringUTF16(IDS_PUSH_MESSAGING_GENERIC_NOTIFICATION_BODY);
  notification_data.tag = kPushMessagingForcedNotificationTag;
  notification_data.icon = GURL();
  notification_data.silent = true;

  content::NotificationDatabaseData database_data;
  database_data.origin = requesting_origin;
  database_data.service_worker_registration_id =
      service_worker_registration_id;
  database_data.notification_data = notification_data;

  scoped_refptr<content::PlatformNotificationContext> notification_context =
    content::BrowserContext::GetStoragePartitionForSite(
        profile_, requesting_origin)->GetPlatformNotificationContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &content::PlatformNotificationContext::WriteNotificationData,
          notification_context,
          requesting_origin, database_data,
          base::Bind(&PushMessagingNotificationManager
                         ::DidWriteNotificationDataIOProxy,
                     weak_factory_.GetWeakPtr(),
                     requesting_origin, notification_data,
                     message_handled_closure)));
}

// static
void PushMessagingNotificationManager::DidWriteNotificationDataIOProxy(
    const base::WeakPtr<PushMessagingNotificationManager>& ui_weak_ptr,
    const GURL& requesting_origin,
    const content::PlatformNotificationData& notification_data,
    const base::Closure& message_handled_closure,
    bool success,
    int64_t persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PushMessagingNotificationManager::DidWriteNotificationData,
                 ui_weak_ptr,
                 requesting_origin, notification_data, message_handled_closure,
                 success, persistent_notification_id));
}

void PushMessagingNotificationManager::DidWriteNotificationData(
    const GURL& requesting_origin,
    const content::PlatformNotificationData& notification_data,
    const base::Closure& message_handled_closure,
    bool success,
    int64_t persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    DLOG(ERROR) << "Writing forced notification to database should not fail";
    message_handled_closure.Run();
    return;
  }
  PlatformNotificationServiceImpl::GetInstance()->DisplayPersistentNotification(
      profile_,
      persistent_notification_id,
      requesting_origin,
      SkBitmap() /* icon */,
      notification_data);
  message_handled_closure.Run();
}
