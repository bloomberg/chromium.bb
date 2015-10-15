// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_manager.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/notification/download_item_notification.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/chromium_strings.h"
#include "content/public/browser/download_item.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

///////////////////////////////////////////////////////////////////////////////
// DownloadNotificationManager implementation:
///////////////////////////////////////////////////////////////////////////////

bool DownloadNotificationManager::IsEnabled() {
  bool enable_download_notification = true;
  std::string arg = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kEnableDownloadNotification);
  if (!arg.empty()) {
    if (arg == "enabled")
      enable_download_notification = true;
    else if (arg == "disabled")
      enable_download_notification = false;
  }
  return enable_download_notification;
}

DownloadNotificationManager::DownloadNotificationManager(Profile* profile)
    : main_profile_(profile),
      items_deleter_(&manager_for_profile_) {
}

DownloadNotificationManager::~DownloadNotificationManager() {
}

void DownloadNotificationManager::OnAllDownloadsRemoving(Profile* profile) {
  DownloadNotificationManagerForProfile* manager_for_profile =
      manager_for_profile_[profile];
  manager_for_profile_.erase(profile);

  base::MessageLoop::current()->DeleteSoon(FROM_HERE, manager_for_profile);
}

void DownloadNotificationManager::OnNewDownloadReady(
    content::DownloadItem* download) {
  Profile* profile = Profile::FromBrowserContext(download->GetBrowserContext());

  if (manager_for_profile_.find(profile) == manager_for_profile_.end()) {
    manager_for_profile_[profile] =
        new DownloadNotificationManagerForProfile(profile, this);
  }

  manager_for_profile_[profile]->OnNewDownloadReady(download);
}

DownloadNotificationManagerForProfile*
DownloadNotificationManager::GetForProfile(Profile* profile) const {
  return manager_for_profile_.at(profile);
}

///////////////////////////////////////////////////////////////////////////////
// DownloadNotificationManagerForProfile implementation:
///////////////////////////////////////////////////////////////////////////////

DownloadNotificationManagerForProfile::DownloadNotificationManagerForProfile(
    Profile* profile,
    DownloadNotificationManager* parent_manager)
    : profile_(profile),
      parent_manager_(parent_manager),
      message_center_(g_browser_process->message_center()),
      items_deleter_(&items_) {
}

DownloadNotificationManagerForProfile::
    ~DownloadNotificationManagerForProfile() {
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

  DownloadItemNotification* item = items_[download];
  items_.erase(download);

  download->RemoveObserver(this);

  // notify
  item->OnDownloadRemoved(download);

  // This removing might be initiated from DownloadNotificationItem, so delaying
  // deleting for item to do remaining cleanups.
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, item);

  if (items_.size() == 0 && parent_manager_)
    parent_manager_->OnAllDownloadsRemoving(profile_);
}

void DownloadNotificationManagerForProfile::OnDownloadDestroyed(
    content::DownloadItem* download) {
  // Do nothing. Cleanup is done in OnDownloadRemoved().
  DownloadItemNotification* item = items_[download];
  items_.erase(download);

  item->OnDownloadRemoved(download);

  // This removing might be initiated from DownloadNotificationItem, so delaying
  // deleting for item to do remaining cleanups.
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, item);

  if (items_.size() == 0 && parent_manager_)
    parent_manager_->OnAllDownloadsRemoving(profile_);
}

void DownloadNotificationManagerForProfile::OnNewDownloadReady(
    content::DownloadItem* download) {
  DCHECK_EQ(profile_,
            Profile::FromBrowserContext(download->GetBrowserContext()));

  download->AddObserver(this);

  for (auto& item : items_) {
    content::DownloadItem* download_item = item.first;
    DownloadItemNotification* download_notification = item.second;
    if (download_item->GetState() == content::DownloadItem::IN_PROGRESS)
      download_notification->DisablePopup();
  }

  DownloadItemNotification* item = new DownloadItemNotification(download, this);
  items_.insert(std::make_pair(download, item));
}

void DownloadNotificationManagerForProfile::OverrideMessageCenterForTest(
    message_center::MessageCenter* message_center) {
  DCHECK(message_center);
  message_center_ = message_center;
}
