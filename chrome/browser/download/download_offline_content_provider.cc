// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_offline_content_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/android/download/download_utils.h"
#endif

using OfflineItemFilter = offline_items_collection::OfflineItemFilter;
using OfflineItemState = offline_items_collection::OfflineItemState;
using OfflineItemProgressUnit =
    offline_items_collection::OfflineItemProgressUnit;
using offline_items_collection::OfflineItemShareInfo;
using OfflineItemVisuals = offline_items_collection::OfflineItemVisuals;
using UpdateDelta = offline_items_collection::UpdateDelta;

namespace {

// Thumbnail size used for generating thumbnails for image files.
const int kThumbnailSizeInDP = 64;

// The delay to wait after loading history and before starting the check for
// externally removed downloads.
const base::TimeDelta kCheckExternallyRemovedDownloadsDelay =
    base::TimeDelta::FromMilliseconds(100);

bool ShouldShowDownloadItem(const DownloadItem* item) {
  return !item->IsTemporary() && !item->IsTransient() && !item->IsDangerous() &&
         !item->GetTargetFilePath().empty();
}

std::unique_ptr<OfflineItemShareInfo> CreateShareInfo(
    const DownloadItem* item) {
  auto share_info = std::make_unique<OfflineItemShareInfo>();
#if defined(OS_ANDROID)
  if (item) {
    share_info->uri =
        DownloadUtils::GetUriStringForPath(item->GetTargetFilePath());
  }
#else
  NOTIMPLEMENTED();
#endif
  return share_info;
}

// Observes the all downloads, primrarily responsible for cleaning up the
// externally removed downloads, and notifying the provider about download
// deletions. Only used for android.
class AllDownloadObserver
    : public download::AllDownloadEventNotifier::Observer {
 public:
  explicit AllDownloadObserver(DownloadOfflineContentProvider* provider);
  ~AllDownloadObserver() override;

  void OnDownloadUpdated(SimpleDownloadManagerCoordinator* manager,
                         DownloadItem* item) override;
  void OnDownloadRemoved(SimpleDownloadManagerCoordinator* manager,
                         DownloadItem* item) override;

 private:
  void DeleteDownloadItem(SimpleDownloadManagerCoordinator* manager,
                          const std::string& guid);

  DownloadOfflineContentProvider* provider_;
  base::WeakPtrFactory<AllDownloadObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AllDownloadObserver);
};

AllDownloadObserver::AllDownloadObserver(
    DownloadOfflineContentProvider* provider)
    : provider_(provider), weak_ptr_factory_(this) {}

AllDownloadObserver::~AllDownloadObserver() {}

void AllDownloadObserver::OnDownloadUpdated(
    SimpleDownloadManagerCoordinator* manager,
    DownloadItem* item) {
  if (item->GetFileExternallyRemoved()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AllDownloadObserver::DeleteDownloadItem,
                                  weak_ptr_factory_.GetWeakPtr(), manager,
                                  item->GetGuid()));
  }
}

void AllDownloadObserver::OnDownloadRemoved(
    SimpleDownloadManagerCoordinator* manager,
    DownloadItem* item) {
  provider_->OnDownloadRemoved(item);
}

void AllDownloadObserver::DeleteDownloadItem(
    SimpleDownloadManagerCoordinator* manager,
    const std::string& guid) {
  DownloadItem* item = manager->GetDownloadByGuid(guid);
  if (item)
    item->Remove();
}

}  // namespace

DownloadOfflineContentProvider::DownloadOfflineContentProvider(
    OfflineContentAggregator* aggregator,
    const std::string& name_space)
    : aggregator_(aggregator),
      name_space_(name_space),
      manager_(nullptr),
      checked_for_externally_removed_downloads_(false) {
  aggregator_->RegisterProvider(name_space_, this);

#if defined(OS_ANDROID)
  all_download_observer_.reset(new AllDownloadObserver(this));
#endif
}

DownloadOfflineContentProvider::~DownloadOfflineContentProvider() {
  aggregator_->UnregisterProvider(name_space_);
  if (manager_) {
    manager_->RemoveObserver(this);
    if (all_download_observer_)
      manager_->GetNotifier()->RemoveObserver(all_download_observer_.get());
  }
}

void DownloadOfflineContentProvider::SetSimpleDownloadManagerCoordinator(
    SimpleDownloadManagerCoordinator* manager) {
  DCHECK(manager);
  DCHECK(!manager_);
  manager_ = manager;
  manager_->AddObserver(this);

  if (all_download_observer_)
    manager_->GetNotifier()->AddObserver(all_download_observer_.get());
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
  if (item) {
    item->DeleteFile(base::DoNothing());
    item->Remove();
  }
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
  std::vector<DownloadItem*> all_items;
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
    GetVisualsOptions options,
    VisualsCallback callback) {
  // TODO(crbug.com/855330) Supply thumbnail if item is visible.
  DownloadItem* item = GetDownload(id.id);
  if (!item || !options.get_icon) {
    // No favicon is available; run the callback without visuals.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), id, nullptr));
    return;
  }

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
    ShareCallback callback) {
  DownloadItem* item = GetDownload(id.id);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), id, CreateShareInfo(item)));
}

void DownloadOfflineContentProvider::OnThumbnailRetrieved(
    const ContentId& id,
    VisualsCallback callback,
    const SkBitmap& bitmap) {
  auto visuals = std::make_unique<OfflineItemVisuals>();
  visuals->icon = gfx::Image::CreateFrom1xBitmap(bitmap);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), id, std::move(visuals)));
}

void DownloadOfflineContentProvider::RenameItem(const ContentId& id,
                                                const std::string& name,
                                                RenameCallback callback) {
  DownloadItem* item = GetDownload(id.id);
  if (!item) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), RenameResult::FAILURE_UNAVAILABLE));
    return;
  }
  download::DownloadItem::RenameDownloadCallback download_callback =
      base::BindOnce(
          &DownloadOfflineContentProvider::OnRenameDownloadCallbackDone,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), item);
  base::FilePath::StringType filename;
#if defined(OS_WIN)
  filename = base::UTF8ToWide(name);
#else
  filename = name;
#endif
  item->Rename(base::FilePath(filename), std::move(download_callback));
}

void DownloadOfflineContentProvider::OnRenameDownloadCallbackDone(
    RenameCallback callback,
    DownloadItem* item,
    DownloadItem::DownloadRenameResult result) {
  if (result == DownloadItem::DownloadRenameResult::SUCCESS) {
    auto offline_item = OfflineItemUtils::CreateOfflineItem(name_space_, item);
    UpdateDelta update_delta;
    update_delta.state_changed = false;
    update_delta.visuals_changed = false;
    UpdateObservers(offline_item, update_delta);
  }

  std::move(callback).Run(
      OfflineItemUtils::ConvertDownloadRenameResultToRenameResult(result));
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

void DownloadOfflineContentProvider::OnDownloadsInitialized(
    bool active_downloads_only) {
  if (active_downloads_only)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &DownloadOfflineContentProvider::CheckForExternallyRemovedDownloads,
          weak_ptr_factory_.GetWeakPtr()),
      kCheckExternallyRemovedDownloadsDelay);
}

void DownloadOfflineContentProvider::OnManagerGoingDown() {
  std::vector<DownloadItem*> all_items;
  GetAllDownloads(&all_items);

  for (auto* item : all_items) {
    if (!ShouldShowDownloadItem(item))
      continue;
    for (auto& observer : observers_)
      observer.OnItemRemoved(ContentId(name_space_, item->GetGuid()));
  }

  manager_ = nullptr;
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

  UpdateDelta update_delta;
  if (item->GetState() == DownloadItem::COMPLETE) {
    update_delta.state_changed = completed_downloads_.find(item->GetGuid()) ==
                                 completed_downloads_.end();

    // TODO(crbug.com/938152): May be move this to DownloadItem.
    // Never call this for completed downloads from history.
    if (completed_downloads_.find(item->GetGuid()) !=
        completed_downloads_.end()) {
      return;
    }

    completed_downloads_.insert(item->GetGuid());

    AddCompletedDownload(item);
  }

  auto offline_item = OfflineItemUtils::CreateOfflineItem(name_space_, item);
  UpdateObservers(offline_item, update_delta);
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

void DownloadOfflineContentProvider::OnDownloadDestroyed(DownloadItem* item) {
  completed_downloads_.erase(item->GetGuid());
}

void DownloadOfflineContentProvider::AddCompletedDownload(DownloadItem* item) {
#if defined(OS_ANDROID)
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
  return manager_ ? manager_->GetDownloadByGuid(download_guid) : nullptr;
}

void DownloadOfflineContentProvider::GetAllDownloads(
    std::vector<DownloadItem*>* all_items) {
  if (manager_)
    manager_->GetAllDownloads(all_items);
}

void DownloadOfflineContentProvider::UpdateObservers(
    const OfflineItem& item,
    const base::Optional<UpdateDelta>& update_delta) {
  for (auto& observer : observers_)
    observer.OnItemUpdated(item, update_delta);
}

void DownloadOfflineContentProvider::CheckForExternallyRemovedDownloads() {
  if (checked_for_externally_removed_downloads_ || !manager_)
    return;

  checked_for_externally_removed_downloads_ = true;

#if defined(OS_ANDROID)
  manager_->CheckForExternallyRemovedDownloads();
#endif
}
