// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_manager.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/notification/download_notification_item.h"
#include "chrome/grit/chromium_strings.h"
#include "content/public/browser/download_item.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using message_center::Notification;

DownloadNotificationManager::DownloadNotificationManager(Profile* profile)
    : items_deleter_(&items_) {
}

DownloadNotificationManager::~DownloadNotificationManager() {
}

void DownloadNotificationManager::OnDownloadStarted(
    DownloadNotificationItem* item) {
  downloading_items_.insert(item);
}

void DownloadNotificationManager::OnDownloadStopped(
    DownloadNotificationItem* item) {
  downloading_items_.erase(item);
}

void DownloadNotificationManager::OnDownloadRemoved(
    DownloadNotificationItem* item) {
  downloading_items_.erase(item);
  items_.erase(item);
}

void DownloadNotificationManager::OnNewDownloadReady(
    content::DownloadItem* download) {
  DownloadNotificationItem* item = new DownloadNotificationItem(download, this);
  items_.insert(item);
}
