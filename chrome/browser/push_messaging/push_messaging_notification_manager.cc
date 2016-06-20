// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_messaging/push_messaging_notification_manager.h"

#include <stddef.h>

#include <bitset>

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/budget_service/background_budget_service.h"
#include "chrome/browser/budget_service/background_budget_service_factory.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_messaging/push_messaging_constants.h"
#include "chrome/common/features.h"
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
#include "content/public/common/notification_resources.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

using content::BrowserThread;
using content::NotificationDatabaseData;
using content::NotificationResources;
using content::PlatformNotificationContext;
using content::PlatformNotificationData;
using content::PushMessagingService;
using content::ServiceWorkerContext;
using content::WebContents;

namespace {
void RecordUserVisibleStatus(content::PushUserVisibleStatus status) {
  UMA_HISTOGRAM_ENUMERATION("PushMessaging.UserVisibleStatus", status,
                            content::PUSH_USER_VISIBLE_STATUS_LAST + 1);
}

content::StoragePartition* GetStoragePartition(Profile* profile,
                                               const GURL& origin) {
  return content::BrowserContext::GetStoragePartitionForSite(profile, origin);
}

NotificationDatabaseData CreateDatabaseData(
    const GURL& origin,
    int64_t service_worker_registration_id) {
  PlatformNotificationData notification_data;
  notification_data.title = url_formatter::FormatUrlForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  notification_data.direction =
      PlatformNotificationData::DIRECTION_LEFT_TO_RIGHT;
  notification_data.body =
      l10n_util::GetStringUTF16(IDS_PUSH_MESSAGING_GENERIC_NOTIFICATION_BODY);
  notification_data.tag = kPushMessagingForcedNotificationTag;
  notification_data.icon = GURL();
  notification_data.silent = true;

  NotificationDatabaseData database_data;
  database_data.origin = origin;
  database_data.service_worker_registration_id = service_worker_registration_id;
  database_data.notification_data = notification_data;
  return database_data;
}

}  // namespace

PushMessagingNotificationManager::PushMessagingNotificationManager(
    Profile* profile)
    : profile_(profile), weak_factory_(this) {}

PushMessagingNotificationManager::~PushMessagingNotificationManager() {}

void PushMessagingNotificationManager::EnforceUserVisibleOnlyRequirements(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(johnme): Relax this heuristic slightly.
  scoped_refptr<PlatformNotificationContext> notification_context =
      GetStoragePartition(profile_, origin)->GetPlatformNotificationContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &PlatformNotificationContext::
              ReadAllNotificationDataForServiceWorkerRegistration,
          notification_context, origin, service_worker_registration_id,
          base::Bind(&PushMessagingNotificationManager::
                         DidGetNotificationsFromDatabaseIOProxy,
                     weak_factory_.GetWeakPtr(), origin,
                     service_worker_registration_id, message_handled_closure)));
}

// static
void PushMessagingNotificationManager::DidGetNotificationsFromDatabaseIOProxy(
    const base::WeakPtr<PushMessagingNotificationManager>& ui_weak_ptr,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure,
    bool success,
    const std::vector<NotificationDatabaseData>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &PushMessagingNotificationManager::DidGetNotificationsFromDatabase,
          ui_weak_ptr, origin, service_worker_registration_id,
          message_handled_closure, success, data));
}

void PushMessagingNotificationManager::DidGetNotificationsFromDatabase(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure,
    bool success,
    const std::vector<NotificationDatabaseData>& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(johnme): Hiding an existing notification should also count as a useful
  // user-visible action done in response to a push message - but make sure that
  // sending two messages in rapid succession which show then hide a
  // notification doesn't count.
  int notification_count = success ? data.size() : 0;
  bool notification_shown = notification_count > 0;
  bool notification_needed = true;

  // Sites with a currently visible tab don't need to show notifications.
#if BUILDFLAG(ANDROID_JAVA_UI)
  for (auto it = TabModelList::begin(); it != TabModelList::end(); ++it) {
    Profile* profile = (*it)->GetProfile();
    WebContents* active_web_contents = (*it)->GetActiveWebContents();
#else
  for (auto* browser : *BrowserList::GetInstance()) {
    Profile* profile = browser->profile();
    WebContents* active_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
#endif
    if (IsTabVisible(profile, active_web_contents, origin)) {
      notification_needed = false;
      break;
    }
  }

  // If more than one notification is showing for this Service Worker, close
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

  // Get the budget for the service worker.
  BackgroundBudgetService* service =
      BackgroundBudgetServiceFactory::GetForProfile(profile_);
  service->GetBudget(
      origin,
      base::Bind(&PushMessagingNotificationManager::DidGetBudget,
                 weak_factory_.GetWeakPtr(), origin,
                 service_worker_registration_id, message_handled_closure,
                 notification_needed, notification_shown));
}

void PushMessagingNotificationManager::DidGetBudget(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure,
    bool notification_needed,
    bool notification_shown,
    const double budget) {
  // Record the budget available any time the budget is queried.
  UMA_HISTOGRAM_COUNTS_100("PushMessaging.BackgroundBudget", budget);

  // Get the site engagement score. Only used for UMA recording.
  SiteEngagementService* ses_service = SiteEngagementService::Get(profile_);
  double ses_score = ses_service->GetScore(origin);

  // Generate histograms for the GetBudget calls which would return "no budget"
  // or "low budget" if an API was available to app developers.
  double cost = BackgroundBudgetService::GetCost(
      BackgroundBudgetService::CostType::SILENT_PUSH);
  if (budget < cost)
    UMA_HISTOGRAM_COUNTS_100("PushMessaging.SESForNoBudgetOrigin", ses_score);
  else if (budget < 2.0 * cost)
    UMA_HISTOGRAM_COUNTS_100("PushMessaging.SESForLowBudgetOrigin", ses_score);

  if (notification_needed && !notification_shown) {
    // If the worker needed to show a notification and didn't, check the budget
    // and take appropriate action.
    CheckForMissedNotification(origin, service_worker_registration_id,
                               message_handled_closure, budget);
    return;
  }

  if (notification_needed && notification_shown) {
    RecordUserVisibleStatus(
        content::PUSH_USER_VISIBLE_STATUS_REQUIRED_AND_SHOWN);
  } else if (!notification_needed && !notification_shown) {
    RecordUserVisibleStatus(
        content::PUSH_USER_VISIBLE_STATUS_NOT_REQUIRED_AND_NOT_SHOWN);
  } else {
    RecordUserVisibleStatus(
        content::PUSH_USER_VISIBLE_STATUS_NOT_REQUIRED_BUT_SHOWN);
  }

  message_handled_closure.Run();
}

bool PushMessagingNotificationManager::IsTabVisible(
    Profile* profile,
    WebContents* active_web_contents,
    const GURL& origin) {
  if (!active_web_contents || !active_web_contents->GetMainFrame())
    return false;

  // Don't leak information from other profiles.
  if (profile != profile_)
    return false;

  // Ignore minimized windows.
  switch (active_web_contents->GetMainFrame()->GetVisibilityState()) {
    case blink::WebPageVisibilityStateHidden:
    case blink::WebPageVisibilityStatePrerender:
      return false;
    case blink::WebPageVisibilityStateVisible:
      break;
  }

  // Use the visible URL since that's the one the user is aware of (and it
  // doesn't matter whether the page loaded successfully).
  GURL visible_url = active_web_contents->GetVisibleURL();

  // view-source: pages are considered to be controlled Service Worker clients
  // and thus should be considered when checking the visible URL. However, the
  // prefix has to be removed before the origins can be compared.
  if (visible_url.SchemeIs(content::kViewSourceScheme))
    visible_url = GURL(visible_url.GetContent());

  return visible_url.GetOrigin() == origin;
}

void PushMessagingNotificationManager::CheckForMissedNotification(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const base::Closure& message_handled_closure,
    const double budget) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the service needed to show a notification but did not, update the
  // budget.
  double cost = BackgroundBudgetService::GetCost(
      BackgroundBudgetService::CostType::SILENT_PUSH);
  if (budget >= cost) {
    RecordUserVisibleStatus(
        content::PUSH_USER_VISIBLE_STATUS_REQUIRED_BUT_NOT_SHOWN_USED_GRACE);

    BackgroundBudgetService* service =
        BackgroundBudgetServiceFactory::GetForProfile(profile_);
    // Update the stored budget.
    service->StoreBudget(origin, budget - cost, message_handled_closure);

    return;
  }

  RecordUserVisibleStatus(
      content::PUSH_USER_VISIBLE_STATUS_REQUIRED_BUT_NOT_SHOWN_GRACE_EXCEEDED);
  rappor::SampleDomainAndRegistryFromGURL(
      g_browser_process->rappor_service(),
      "PushMessaging.GenericNotificationShown.Origin", origin);

  // The site failed to show a notification when one was needed, and they don't
  // have enough budget to cover the cost of suppressing, so we will show a
  // generic notification.
  NotificationDatabaseData database_data =
      CreateDatabaseData(origin, service_worker_registration_id);
  scoped_refptr<PlatformNotificationContext> notification_context =
      GetStoragePartition(profile_, origin)->GetPlatformNotificationContext();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&PlatformNotificationContext::WriteNotificationData,
                 notification_context, origin, database_data,
                 base::Bind(&PushMessagingNotificationManager::
                                DidWriteNotificationDataIOProxy,
                            weak_factory_.GetWeakPtr(), origin,
                            database_data.notification_data,
                            message_handled_closure)));
}

// static
void PushMessagingNotificationManager::DidWriteNotificationDataIOProxy(
    const base::WeakPtr<PushMessagingNotificationManager>& ui_weak_ptr,
    const GURL& origin,
    const PlatformNotificationData& notification_data,
    const base::Closure& message_handled_closure,
    bool success,
    int64_t persistent_notification_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&PushMessagingNotificationManager::DidWriteNotificationData,
                 ui_weak_ptr, origin, notification_data,
                 message_handled_closure, success, persistent_notification_id));
}

void PushMessagingNotificationManager::DidWriteNotificationData(
    const GURL& origin,
    const PlatformNotificationData& notification_data,
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
      profile_, persistent_notification_id, origin, notification_data,
      NotificationResources());

  message_handled_closure.Run();
}
