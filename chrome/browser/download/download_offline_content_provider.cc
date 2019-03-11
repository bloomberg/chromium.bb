// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_offline_content_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/download/image_thumbnail_request.h"
#include "chrome/browser/download/offline_item_utils.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/browser_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/download_manager_bridge.h"
#include "chrome/browser/android/download/download_manager_service.h"
#endif

using OfflineItemFilter = offline_items_collection::OfflineItemFilter;
using OfflineItemState = offline_items_collection::OfflineItemState;
using OfflineItemProgressUnit =
    offline_items_collection::OfflineItemProgressUnit;
using OfflineItemVisuals = offline_items_collection::OfflineItemVisuals;

namespace {

// Thumbnail size used for generating thumbnails for image files.
const int kThumbnailSizeInDP = 64;

bool ShouldShowDownloadItem(const DownloadItem* item) {
  return !item->IsTemporary() && !item->IsTransient() && !item->IsDangerous() &&
         !item->GetTargetFilePath().empty();
}

}  // namespace

DownloadOfflineContentProvider::DownloadOfflineContentProvider(
    OfflineContentAggregator* aggregator,
    const std::string& name_space)
    : aggregator_(aggregator),
      name_space_(name_space),
      manager_(nullptr),
      weak_ptr_factory_(this) {
  aggregator_->RegisterProvider(name_space_, this);
}

DownloadOfflineContentProvider::~DownloadOfflineContentProvider() {
  aggregator_->UnregisterProvider(name_space_);
}

void DownloadOfflineContentProvider::SetDownloadManager(
    DownloadManager* manager) {
  manager_ = manager;
}

// TODO(shaktisahu) : Pass DownloadOpenSource.
void DownloadOfflineContentProvider::OpenItem(LaunchLocation location,
                                              const ContentId& id) {
  DownloadItem* item = GetDownload(id.id);
  if (item)
    item->OpenDownload();
}

void DownloadOfflineContentProvider::RemoveItem(const ContentId& id) {
  DownloadItem* item = GetDownload(id.id);
  if (item)
    item->Remove();
}

void DownloadOfflineContentProvider::CancelDownload(const ContentId& id) {
  DownloadItem* item = GetDownload(id.id);
  if (item)
    item->Cancel(true);
}

void DownloadOfflineContentProvider::PauseDownload(const ContentId& id) {
  DownloadItem* item = GetDownload(id.id);
  if (item)
    item->Pause();
}

void DownloadOfflineContentProvider::ResumeDownload(const ContentId& id,
                                                    bool has_user_gesture) {
  DownloadItem* item = GetDownload(id.id);
  if (item)
    item->Resume(has_user_gesture);
}

void DownloadOfflineContentProvider::GetItemById(
    const ContentId& id,
    OfflineContentProvider::SingleItemCallback callback) {
  DownloadItem* item = GetDownload(id.id);
  auto offline_item =
      item && ShouldShowDownloadItem(item)
          ? base::make_optional(
                OfflineItemUtils::CreateOfflineItem(name_space_, item))
          : base::nullopt;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), offline_item));
}

void DownloadOfflineContentProvider::GetAllItems(
    OfflineContentProvider::MultipleItemCallback callback) {
  DownloadManager::DownloadVector all_items;
  GetAllDownloads(&all_items);

  std::vector<OfflineItem> items;
  for (auto* item : all_items) {
    if (!ShouldShowDownloadItem(item))
      continue;

    items.push_back(OfflineItemUtils::CreateOfflineItem(name_space_, item));
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), items));
}

void DownloadOfflineContentProvider::GetVisualsForItem(
    const ContentId& id,
    VisualsCallback callback) {
  // TODO(crbug.com/855330) Supply thumbnail if item is visible.
  DownloadItem* item = GetDownload(id.id);
  if (!item)
    return;

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  int icon_size = kThumbnailSizeInDP * display.device_scale_factor();

  auto request = std::make_unique<ImageThumbnailRequest>(
      icon_size,
      base::BindOnce(&DownloadOfflineContentProvider::OnThumbnailRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), id, std::move(callback)));
  request->Start(item->GetTargetFilePath());

  // Dropping ownership of |request| here because it will clean itself up once
  // the started request finishes.
  request.release();
}

void DownloadOfflineContentProvider::GetShareInfoForItem(
    const ContentId& id,
    ShareCallback callback) {}

void DownloadOfflineContentProvider::OnThumbnailRetrieved(
    const ContentId& id,
    VisualsCallback callback,
    const SkBitmap& bitmap) {
  auto visuals = std::make_unique<OfflineItemVisuals>();
  visuals->icon = gfx::Image::CreateFrom1xBitmap(bitmap);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), id, std::move(visuals)));
}

void DownloadOfflineContentProvider::AddObserver(
    OfflineContentProvider::Observer* observer) {
  if (observers_.HasObserver(observer))
    return;
  observers_.AddObserver(observer);
}

void DownloadOfflineContentProvider::RemoveObserver(
    OfflineContentProvider::Observer* observer) {
  if (!observers_.HasObserver(observer))
    return;

  observers_.RemoveObserver(observer);
}

void DownloadOfflineContentProvider::OnDownloadStarted(DownloadItem* item) {
  item->RemoveObserver(this);
  item->AddObserver(this);

  OnDownloadUpdated(item);
}

void DownloadOfflineContentProvider::OnDownloadUpdated(DownloadItem* item) {
  // Wait until the target path is determined or the download is canceled.
  if (item->GetTargetFilePath().empty() &&
      item->GetState() != DownloadItem::CANCELLED)
    return;

  if (!ShouldShowDownloadItem(item))
    return;

  if (item->GetState() == DownloadItem::COMPLETE) {
    // TODO(crbug.com/938152): May be move this to DownloadItem.
    AddCompletedDownload(item);
  }

  UpdateObservers(item);
}

void DownloadOfflineContentProvider::OnDownloadRemoved(DownloadItem* item) {
  if (!ShouldShowDownloadItem(item))
    return;

#if defined(OS_ANDROID)
  DownloadManagerBridge::RemoveCompletedDownload(item);
#endif

  ContentId contentId(name_space_, item->GetGuid());
  for (auto& observer : observers_)
    observer.OnItemRemoved(contentId);
}

void DownloadOfflineContentProvider::AddCompletedDownload(DownloadItem* item) {
#if defined(OS_ANDROID)
  if (completed_downloads_.find(item->GetGuid()) != completed_downloads_.end())
    return;
  completed_downloads_.insert(item->GetGuid());

  DownloadManagerBridge::AddCompletedDownload(
      item,
      base::BindOnce(&DownloadOfflineContentProvider::AddCompletedDownloadDone,
                     weak_ptr_factory_.GetWeakPtr(), item));
#endif
}

void DownloadOfflineContentProvider::AddCompletedDownloadDone(
    DownloadItem* item,
    int64_t system_download_id,
    bool can_resolve) {
  if (can_resolve && item->HasUserGesture())
    item->OpenDownload();
}

DownloadItem* DownloadOfflineContentProvider::GetDownload(
    const std::string& download_guid) {
#if defined(OS_ANDROID)
  bool incognito =
      manager_ ? manager_->GetBrowserContext()->IsOffTheRecord() : false;
  return DownloadManagerService::GetInstance()->GetDownload(download_guid,
                                                            incognito);
#else
  return manager_->GetDownloadByGuid(download_guid);
#endif
}

void DownloadOfflineContentProvider::GetAllDownloads(
    DownloadManager::DownloadVector* all_items) {
#if defined(OS_ANDROID)
  bool incognito =
      manager_ ? manager_->GetBrowserContext()->IsOffTheRecord() : false;
  DownloadManagerService::GetInstance()->GetAllDownloads(all_items, incognito);
#else
  manager_->GetAllDownloads(all_items);
#endif
}

void DownloadOfflineContentProvider::UpdateObservers(DownloadItem* item) {
  for (auto& observer : observers_) {
    observer.OnItemUpdated(
        OfflineItemUtils::CreateOfflineItem(name_space_, item));
  }
}
