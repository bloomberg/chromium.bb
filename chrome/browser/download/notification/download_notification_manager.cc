// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_manager.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/notification/download_item_notification.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/download_item.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

namespace {

class DownloadNotificationHandler : public NotificationHandler {
 public:
  explicit DownloadNotificationHandler(
      DownloadNotificationManagerForProfile* manager)
      : manager_(manager) {}
  ~DownloadNotificationHandler() override = default;

  void OnClose(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               bool by_user,
               base::OnceClosure completed_closure) override {
    if (by_user) {
      manager_->GetNotificationItemByGuid(notification_id)
          ->OnNotificationClose();
    }

    std::move(completed_closure).Run();
  }

  void OnClick(Profile* profile,
               const GURL& origin,
               const std::string& notification_id,
               const base::Optional<int>& action_index,
               const base::Optional<base::string16>& reply,
               base::OnceClosure completed_closure) override {
    DownloadItemNotification* item =
        manager_->GetNotificationItemByGuid(notification_id);
    if (!action_index)
      item->OnNotificationClick();
    else
      item->OnNotificationButtonClick(*action_index);

    std::move(completed_closure).Run();
  }

 private:
  DownloadNotificationManagerForProfile* manager_;

  DISALLOW_COPY_AND_ASSIGN(DownloadNotificationHandler);
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DownloadNotificationManager implementation:
///////////////////////////////////////////////////////////////////////////////

DownloadNotificationManager::DownloadNotificationManager(Profile* profile)
    : main_profile_(profile) {}

DownloadNotificationManager::~DownloadNotificationManager() = default;

void DownloadNotificationManager::OnAllDownloadsRemoving(Profile* profile) {
  manager_for_profile_.erase(profile);
}

void DownloadNotificationManager::OnNewDownloadReady(
    content::DownloadItem* download) {
  Profile* profile = Profile::FromBrowserContext(download->GetBrowserContext());

  if (manager_for_profile_.find(profile) == manager_for_profile_.end()) {
    manager_for_profile_[profile] =
        base::MakeUnique<DownloadNotificationManagerForProfile>(profile, this);
  }

  manager_for_profile_[profile]->OnNewDownloadReady(download);
}

DownloadNotificationManagerForProfile*
DownloadNotificationManager::GetForProfile(Profile* profile) const {
  return manager_for_profile_.at(profile).get();
}

///////////////////////////////////////////////////////////////////////////////
// DownloadNotificationManagerForProfile implementation:
///////////////////////////////////////////////////////////////////////////////

DownloadNotificationManagerForProfile::DownloadNotificationManagerForProfile(
    Profile* profile,
    DownloadNotificationManager* parent_manager)
    : profile_(profile), parent_manager_(parent_manager) {
  NotificationDisplayService* service =
      NotificationDisplayServiceFactory::GetForProfile(profile);
  DCHECK(!service->GetNotificationHandler(NotificationHandler::Type::DOWNLOAD));
  service->AddNotificationHandler(
      NotificationHandler::Type::DOWNLOAD,
      std::make_unique<DownloadNotificationHandler>(this));
}

DownloadNotificationManagerForProfile::
    ~DownloadNotificationManagerForProfile() {
  NotificationDisplayServiceFactory::GetForProfile(profile_)
      ->RemoveNotificationHandler(NotificationHandler::Type::DOWNLOAD);
  for (const auto& download : items_) {
    download.first->RemoveObserver(this);
  }
}

void DownloadNotificationManagerForProfile::OnDownloadUpdated(
    content::DownloadItem* changed_download) {
  DCHECK(items_.find(changed_download) != items_.end());

  items_[changed_download]->OnDownloadUpdated(changed_download);
}

void DownloadNotificationManagerForProfile::OnDownloadOpened(
    content::DownloadItem* changed_download) {
  items_[changed_download]->OnDownloadUpdated(changed_download);
}

void DownloadNotificationManagerForProfile::OnDownloadRemoved(
    content::DownloadItem* download) {
  DCHECK(items_.find(download) != items_.end());

  std::unique_ptr<DownloadItemNotification> item = std::move(items_[download]);
  items_.erase(download);

  download->RemoveObserver(this);

  // notify
  item->OnDownloadRemoved(download);

  // This removing might be initiated from DownloadNotificationItem, so delaying
  // deleting for item to do remaining cleanups.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, item.release());

  if (items_.size() == 0 && parent_manager_)
    parent_manager_->OnAllDownloadsRemoving(profile_);

  // |this| is deleted.
}

void DownloadNotificationManagerForProfile::OnDownloadDestroyed(
    content::DownloadItem* download) {
  // Do nothing. Cleanup is done in OnDownloadRemoved().
  std::unique_ptr<DownloadItemNotification> item = std::move(items_[download]);
  items_.erase(download);

  item->OnDownloadRemoved(download);

  // This removing might be initiated from DownloadNotificationItem, so delaying
  // deleting for item to do remaining cleanups.
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, item.release());

  if (items_.size() == 0 && parent_manager_)
    parent_manager_->OnAllDownloadsRemoving(profile_);

  // |this| is deleted.
}

void DownloadNotificationManagerForProfile::OnNewDownloadReady(
    content::DownloadItem* download) {
  DCHECK_EQ(profile_,
            Profile::FromBrowserContext(download->GetBrowserContext()));

  download->AddObserver(this);

  for (auto& item : items_) {
    content::DownloadItem* download_item = item.first;
    DownloadItemNotification* download_notification = item.second.get();
    if (download_item->GetState() == content::DownloadItem::IN_PROGRESS)
      download_notification->DisablePopup();
  }

  items_[download] = std::make_unique<DownloadItemNotification>(download, this);
}

DownloadItemNotification*
DownloadNotificationManagerForProfile::GetNotificationItemByGuid(
    const std::string& guid) {
  for (auto& item : items_) {
    if (item.first->GetGuid() == guid)
      return item.second.get();
  }

  NOTREACHED();
  return nullptr;
}
