// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notifications/platform_notification_context_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "content/browser/notifications/blink_notification_service_impl.h"
#include "content/browser/notifications/notification_database.h"
#include "content/browser/notifications/notification_trigger_constants.h"
#include "content/browser/notifications/platform_notification_service_proxy.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_database_data.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/notifications/notification_resources.h"

namespace content {
namespace {

// Name of the directory in the user's profile directory where the notification
// database files should be stored.
const base::FilePath::CharType kPlatformNotificationsDirectory[] =
    FILE_PATH_LITERAL("Platform Notifications");

// Checks if this notification can trigger in the future.
bool CanTrigger(const NotificationDatabaseData& data) {
  if (!base::FeatureList::IsEnabled(features::kNotificationTriggers))
    return false;
  return data.notification_data.show_trigger_timestamp && !data.has_triggered;
}

}  // namespace

PlatformNotificationContextImpl::PlatformNotificationContextImpl(
    const base::FilePath& path,
    BrowserContext* browser_context,
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : path_(path),
      browser_context_(browser_context),
      service_worker_context_(service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PlatformNotificationContextImpl::~PlatformNotificationContextImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the database has been initialized, it must be deleted on the task runner
  // thread as closing it may cause file I/O.
  if (database_) {
    DCHECK(task_runner_);
    task_runner_->DeleteSoon(FROM_HERE, database_.release());
  }
}

void PlatformNotificationContextImpl::Initialize() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service_proxy_ = std::make_unique<PlatformNotificationServiceProxy>(
      service_worker_context_, browser_context_);

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();
  if (!service) {
    std::set<std::string> displayed_notifications;
    DidGetNotifications(std::move(displayed_notifications), false);
    return;
  }

  ukm_callback_ = base::BindRepeating(
      &PlatformNotificationService::RecordNotificationUkmEvent,
      base::Unretained(service), browser_context_);

  service->GetDisplayedNotifications(
      browser_context_,
      base::BindOnce(&PlatformNotificationContextImpl::DidGetNotifications,
                     this));
}

void PlatformNotificationContextImpl::DidGetNotifications(
    std::set<std::string> displayed_notifications,
    bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Check if there are pending notifications to display.
  base::Time next_trigger = base::Time::Max();
  if (service_proxy_ &&
      base::FeatureList::IsEnabled(features::kNotificationTriggers)) {
    next_trigger = service_proxy_->GetNextTrigger();
  }

  // Synchronize the notifications stored in the database with the set of
  // displaying notifications in |displayed_notifications|. This is necessary
  // because flakiness may cause a platform to inform Chrome of a notification
  // that has since been closed, or because the platform does not support
  // notifications that exceed the lifetime of the browser process.
  if (supports_synchronization || next_trigger <= base::Time::Now()) {
    LazyInitialize(base::BindOnce(
        &PlatformNotificationContextImpl::DoSyncNotificationData, this,
        supports_synchronization, std::move(displayed_notifications)));
  } else if (service_proxy_ && next_trigger != base::Time::Max()) {
    service_proxy_->ScheduleTrigger(next_trigger);
  }

  // |service_worker_context_| may be NULL in tests.
  if (service_worker_context_)
    service_worker_context_->AddObserver(this);
}

void PlatformNotificationContextImpl::DoSyncNotificationData(
    bool supports_synchronization,
    std::set<std::string> displayed_notifications,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized)
    return;

  // Reset |next_trigger_| to keep track of the next trigger timestamp.
  next_trigger_ = base::nullopt;

  // Iterate over all notifications and delete all expired ones.
  NotificationDatabase::Status status =
      database_->ForEachNotificationData(base::BindRepeating(
          &PlatformNotificationContextImpl::DoHandleSyncNotification, this,
          supports_synchronization, displayed_notifications));

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  // Schedule the next trigger timestamp.
  if (next_trigger_ && service_proxy_)
    service_proxy_->ScheduleTrigger(next_trigger_.value());
}

void PlatformNotificationContextImpl::DoHandleSyncNotification(
    bool supports_synchronization,
    const std::set<std::string>& displayed_notifications,
    const NotificationDatabaseData& data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Handle pending notifications.
  if (CanTrigger(data)) {
    base::Time timestamp =
        data.notification_data.show_trigger_timestamp.value();
    // Check if we should display this notification.
    if (timestamp <= base::Time::Now())
      DoTriggerNotification(data);
    else if (!next_trigger_ || next_trigger_.value() > timestamp)
      next_trigger_ = timestamp;
    return;
  }

  // Do not delete notifications if the platform does not support syncing them.
  if (!supports_synchronization)
    return;

  // Delete notifications that are not on screen anymore.
  if (!displayed_notifications.count(data.notification_id))
    database_->DeleteNotificationData(data.notification_id, data.origin);
}

void PlatformNotificationContextImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  service_proxy_.reset();
  services_.clear();

  // |service_worker_context_| may be NULL in tests.
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

void PlatformNotificationContextImpl::CreateService(
    const url::Origin& origin,
    blink::mojom::NotificationServiceRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  services_.push_back(std::make_unique<BlinkNotificationServiceImpl>(
      this, browser_context_, service_worker_context_, origin,
      std::move(request)));
}

void PlatformNotificationContextImpl::RemoveService(
    BlinkNotificationServiceImpl* service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::EraseIf(
      services_,
      [service](const std::unique_ptr<BlinkNotificationServiceImpl>& ptr) {
        return ptr.get() == service;
      });
}

void PlatformNotificationContextImpl::ReadNotificationDataAndRecordInteraction(
    const std::string& notification_id,
    const GURL& origin,
    const PlatformNotificationContext::Interaction interaction,
    ReadResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoReadNotificationData, this,
      notification_id, origin, interaction, std::move(callback)));
}

void PlatformNotificationContextImpl::DoReadNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    Interaction interaction,
    ReadResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), /* success= */ false,
                       NotificationDatabaseData()));
    return;
  }

  NotificationDatabaseData database_data;
  NotificationDatabase::Status status =
      database_->ReadNotificationDataAndRecordInteraction(
          notification_id, origin, interaction, &database_data);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.ReadResult", status,
                            NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), /* success= */ true,
                       database_data));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(std::move(callback), /* success= */ false,
                     NotificationDatabaseData()));
}

void PlatformNotificationContextImpl::TriggerNotifications() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::set<std::string> displayed_notifications;
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoSyncNotificationData, this,
      /* supports_synchronization= */ false,
      std::move(displayed_notifications)));
}

void PlatformNotificationContextImpl::DoTriggerNotification(
    const NotificationDatabaseData& database_data) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Bail out in case we can not display the notification after Shutdown.
  if (!service_proxy_)
    return;

  blink::NotificationResources resources;
  NotificationDatabase::Status status = database_->ReadNotificationResources(
      database_data.notification_id, database_data.origin, &resources);

  if (status != NotificationDatabase::STATUS_OK) {
    // TODO(knollr): add UMA for this
    resources = blink::NotificationResources();
  }

  // Create a copy of the |database_data| to store the |has_triggered| flag.
  NotificationDatabaseData write_database_data = database_data;
  write_database_data.has_triggered = true;
  status = database_->WriteNotificationData(write_database_data.origin,
                                            write_database_data);

  if (status != NotificationDatabase::STATUS_OK) {
    database_->DeleteNotificationData(write_database_data.notification_id,
                                      write_database_data.origin);
    return;
  }

  write_database_data.notification_resources = std::move(resources);
  service_proxy_->DisplayNotification(std::move(write_database_data),
                                      base::DoNothing());
}

void PlatformNotificationContextImpl::ReadNotificationResources(
    const std::string& notification_id,
    const GURL& origin,
    ReadResourcesResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoReadNotificationResources, this,
      notification_id, origin, std::move(callback)));
}

void PlatformNotificationContextImpl::DoReadNotificationResources(
    const std::string& notification_id,
    const GURL& origin,
    ReadResourcesResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), /* success= */ false,
                       blink::NotificationResources()));
    return;
  }

  blink::NotificationResources notification_resources;
  NotificationDatabase::Status status = database_->ReadNotificationResources(
      notification_id, origin, &notification_resources);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.ReadResult", status,
                            NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), /* success= */ true,
                       notification_resources));
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(std::move(callback), /* success= */ false,
                     blink::NotificationResources()));
}

void PlatformNotificationContextImpl::
    SynchronizeDisplayedNotificationsForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        ReadAllResultCallback callback,
        std::set<std::string> notification_ids,
        bool supports_synchronization) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::
          DoReadAllNotificationDataForServiceWorkerRegistration,
      this, origin, service_worker_registration_id, std::move(callback),
      std::move(notification_ids), supports_synchronization));
}

void PlatformNotificationContextImpl::
    ReadAllNotificationDataForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        ReadAllResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PlatformNotificationService* service =
      GetContentClient()->browser()->GetPlatformNotificationService();

  if (!service) {
    // Rely on the database only
    std::set<std::string> notification_ids;
    SynchronizeDisplayedNotificationsForServiceWorkerRegistration(
        origin, service_worker_registration_id, std::move(callback),
        std::move(notification_ids), /* supports_synchronization= */ false);
    return;
  }

  service->GetDisplayedNotifications(
      browser_context_,
      base::BindOnce(
          &PlatformNotificationContextImpl::
              SynchronizeDisplayedNotificationsForServiceWorkerRegistration,
          this, origin, service_worker_registration_id, std::move(callback)));
}

void PlatformNotificationContextImpl::
    DoReadAllNotificationDataForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        ReadAllResultCallback callback,
        std::set<std::string> displayed_notifications,
        bool supports_synchronization,
        bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), /* success= */ false,
                       std::vector<NotificationDatabaseData>()));
    return;
  }

  std::vector<NotificationDatabaseData> notification_datas;

  NotificationDatabase::Status status =
      database_->ReadAllNotificationDataForServiceWorkerRegistration(
          origin, service_worker_registration_id, &notification_datas);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.ReadForServiceWorkerResult",
                            status, NotificationDatabase::STATUS_COUNT);

  std::vector<std::string> obsolete_notifications;

  if (status == NotificationDatabase::STATUS_OK) {
    if (supports_synchronization) {
      for (auto it = notification_datas.begin();
           it != notification_datas.end();) {
        // The database is only used for persistent notifications.
        DCHECK(NotificationIdGenerator::IsPersistentNotification(
            it->notification_id));
        if (displayed_notifications.count(it->notification_id) ||
            CanTrigger(*it)) {
          ++it;
        } else {
          obsolete_notifications.push_back(it->notification_id);
          it = notification_datas.erase(it);
        }
      }
    }

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), /* success= */ true,
                       notification_datas));

    // Remove notifications that are not actually on display anymore.
    for (const auto& it : obsolete_notifications)
      database_->DeleteNotificationData(it, origin);
    return;
  }

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(std::move(callback), /* success= */ false,
                     std::vector<NotificationDatabaseData>()));
}

void PlatformNotificationContextImpl::WriteNotificationData(
    int64_t persistent_notification_id,
    int64_t service_worker_registration_id,
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    WriteResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::DoWriteNotificationData, this,
      service_worker_registration_id, persistent_notification_id, origin,
      database_data, std::move(callback)));
}

bool PlatformNotificationContextImpl::DoCheckNotificationTriggerQuota(
    const GURL& origin) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  int notification_count = 0;
  // Iterate over all notifications and count all scheduled notifications for
  // |origin|.
  NotificationDatabase::Status status =
      database_->ForEachNotificationData(base::BindRepeating(
          [](const GURL& expected_origin, int* count,
             const NotificationDatabaseData& data) {
            if (CanTrigger(data) && data.origin == expected_origin)
              *count = *count + 1;
          },
          origin, &notification_count));

  // Blow away the database if reading data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  return notification_count < kMaximumScheduledNotificationsPerOrigin;
}

void PlatformNotificationContextImpl::DoWriteNotificationData(
    int64_t service_worker_registration_id,
    int64_t persistent_notification_id,
    const GURL& origin,
    const NotificationDatabaseData& database_data,
    WriteResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_data.notification_id.empty());
  if (!initialized) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), /* success= */ false,
                       /* notification_id= */ ""));
    return;
  }

  bool replaces_existing = false;
  std::string notification_id =
      notification_id_generator_.GenerateForPersistentNotification(
          origin, database_data.notification_data.tag,
          persistent_notification_id);

  // Eagerly delete data for replaced notifications from the database.
  if (!database_data.notification_data.tag.empty()) {
    std::set<std::string> deleted_notification_ids;
    NotificationDatabase::Status delete_status =
        database_->DeleteAllNotificationDataForOrigin(
            origin, database_data.notification_data.tag,
            &deleted_notification_ids);

    replaces_existing = deleted_notification_ids.count(notification_id) != 0;

    UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DeleteBeforeWriteResult",
                              delete_status,
                              NotificationDatabase::STATUS_COUNT);

    // Unless the database was corrupted following this change, there is no
    // reason to bail out here in event of failure because the notification
    // display logic will handle notification replacement for the user.
    if (delete_status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
      DestroyDatabase();

      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
          base::BindOnce(std::move(callback), /* success= */ false,
                         /* notification_id= */ ""));
      return;
    }
  }

  // Create a copy of the |database_data| to store a generated notification ID.
  NotificationDatabaseData write_database_data = database_data;
  write_database_data.notification_id = notification_id;
  write_database_data.origin = origin;

  if (CanTrigger(write_database_data) &&
      !DoCheckNotificationTriggerQuota(origin)) {
    // TODO(knollr): Reply with a custom error so developers can handle this.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), /* success= */ false,
                       /* notification_id= */ ""));
    return;
  }

  NotificationDatabase::Status status =
      database_->WriteNotificationData(origin, write_database_data);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.WriteResult", status,
                            NotificationDatabase::STATUS_COUNT);

  if (status == NotificationDatabase::STATUS_OK) {
    if (CanTrigger(write_database_data) && service_proxy_) {
      if (replaces_existing)
        service_proxy_->CloseNotification(notification_id);
      // Schedule notification to be shown.
      service_proxy_->ScheduleTrigger(
          write_database_data.notification_data.show_trigger_timestamp.value());
    }

    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), /* success= */ true,
                       write_database_data.notification_id));

    return;
  }

  // Blow away the database if writing data failed due to corruption.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(std::move(callback), /* success= */ false,
                     /* notification_id= */ ""));
}

void PlatformNotificationContextImpl::DeleteNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    DeleteResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  LazyInitialize(
      base::BindOnce(&PlatformNotificationContextImpl::DoDeleteNotificationData,
                     this, notification_id, origin, std::move(callback)));
}

void PlatformNotificationContextImpl::DoDeleteNotificationData(
    const std::string& notification_id,
    const GURL& origin,
    DeleteResultCallback callback,
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
        base::BindOnce(std::move(callback), false));
    return;
  }

  NotificationDatabase::Status status =
      database_->DeleteNotificationData(notification_id, origin);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DeleteResult", status,
                            NotificationDatabase::STATUS_COUNT);

  bool success = status == NotificationDatabase::STATUS_OK;

  // Blow away the database if deleting data failed due to corruption. Following
  // the contract of the delete methods, consider this to be a success as the
  // caller's goal has been achieved: the data is gone.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
    DestroyDatabase();
    success = true;
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
      base::BindOnce(std::move(callback), success));
}

void PlatformNotificationContextImpl::OnRegistrationDeleted(
    int64_t registration_id,
    const GURL& pattern) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(
      base::BindOnce(&PlatformNotificationContextImpl::
                         DoDeleteNotificationsForServiceWorkerRegistration,
                     this, pattern.GetOrigin(), registration_id));
}

void PlatformNotificationContextImpl::
    DoDeleteNotificationsForServiceWorkerRegistration(
        const GURL& origin,
        int64_t service_worker_registration_id,
        bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized)
    return;

  std::set<std::string> deleted_notification_ids;
  NotificationDatabase::Status status =
      database_->DeleteAllNotificationDataForServiceWorkerRegistration(
          origin, service_worker_registration_id, &deleted_notification_ids);

  UMA_HISTOGRAM_ENUMERATION(
      "Notifications.Database.DeleteServiceWorkerRegistrationResult", status,
      NotificationDatabase::STATUS_COUNT);

  // Blow away the database if a corruption error occurred during the deletion.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED)
    DestroyDatabase();

  // TODO(peter): Close the notifications in |deleted_notification_ids|. See
  // https://crbug.com/532436.
}

void PlatformNotificationContextImpl::OnStorageWiped() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  LazyInitialize(base::BindOnce(
      &PlatformNotificationContextImpl::OnStorageWipedInitialized, this));
}

void PlatformNotificationContextImpl::OnStorageWipedInitialized(
    bool initialized) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!initialized)
    return;
  DestroyDatabase();
}

void PlatformNotificationContextImpl::LazyInitialize(
    InitializeResultCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!task_runner_) {
    task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&PlatformNotificationContextImpl::OpenDatabase,
                                this, std::move(callback)));
}

void PlatformNotificationContextImpl::OpenDatabase(
    InitializeResultCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (database_) {
    std::move(callback).Run(/* initialized= */ true);
    return;
  }

  database_.reset(new NotificationDatabase(GetDatabasePath(), ukm_callback_));
  NotificationDatabase::Status status =
      database_->Open(/* create_if_missing= */ true);

  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.OpenResult", status,
                            NotificationDatabase::STATUS_COUNT);

  // When the database could not be opened due to corruption, destroy it, blow
  // away the contents of the directory and try re-opening the database.
  if (status == NotificationDatabase::STATUS_ERROR_CORRUPTED) {
    if (DestroyDatabase()) {
      database_.reset(
          new NotificationDatabase(GetDatabasePath(), ukm_callback_));
      status = database_->Open(/* create_if_missing= */ true);

      UMA_HISTOGRAM_ENUMERATION(
          "Notifications.Database.OpenAfterCorruptionResult", status,
          NotificationDatabase::STATUS_COUNT);
    }
  }

  if (status == NotificationDatabase::STATUS_OK) {
    std::move(callback).Run(/* initialized= */ true);
    return;
  }

  database_.reset();

  std::move(callback).Run(/* initialized= */ false);
}

bool PlatformNotificationContextImpl::DestroyDatabase() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(database_);

  NotificationDatabase::Status status = database_->Destroy();
  UMA_HISTOGRAM_ENUMERATION("Notifications.Database.DestroyResult", status,
                            NotificationDatabase::STATUS_COUNT);

  database_.reset();

  // TODO(peter): Close any existing persistent notifications on the platform.

  // Remove all files in the directory that the database was previously located
  // in, to make sure that any left-over files are gone as well.
  base::FilePath database_path = GetDatabasePath();
  if (!database_path.empty())
    return base::DeleteFile(database_path, true);

  return true;
}

base::FilePath PlatformNotificationContextImpl::GetDatabasePath() const {
  if (path_.empty())
    return path_;

  return path_.Append(kPlatformNotificationsDirectory);
}

void PlatformNotificationContextImpl::SetTaskRunnerForTesting(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner) {
  task_runner_ = task_runner;
}

}  // namespace content
