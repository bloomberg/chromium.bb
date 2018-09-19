// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_manager.h"

#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/notification/download_item_notification.h"

DownloadNotificationManager::DownloadNotificationManager(Profile* profile)
    : profile_(profile) {}

DownloadNotificationManager::~DownloadNotificationManager() = default;

void DownloadNotificationManager::OnNewDownloadReady(
    download::DownloadItem* item) {
  // Lower the priority of all existing in-progress download notifications.
  for (auto& item : items_) {
    DownloadUIModel* model = item.second->GetDownload();
    DownloadItemNotification* download_notification = item.second.get();
    if (model->GetState() == download::DownloadItem::IN_PROGRESS)
      download_notification->DisablePopup();
  }

  std::unique_ptr<DownloadUIModel> model =
      std::make_unique<DownloadItemModel>(item);
  ContentId contentId = model->GetContentId();
  items_[contentId] =
      std::make_unique<DownloadItemNotification>(profile_, std::move(model));
  items_[contentId]->SetObserver(this);
}

void DownloadNotificationManager::OnDownloadDestroyed(
    const ContentId& contentId) {
  items_.erase(contentId);
}
