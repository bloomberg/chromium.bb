// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/favicon_cache.h"

#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "sync/api/time.h"
#include "ui/gfx/favicon_size.h"

namespace browser_sync {

// Synced favicon storage and tracking.
// Note: we don't use the favicon service for storing these because these
// favicons are not necessarily associated with any local navigation, and
// hence would not work with the current expiration logic. We have custom
// expiration logic based on visit time/bookmark status/etc.
// See crbug.com/122890.
struct SyncedFaviconInfo {
  explicit SyncedFaviconInfo(const GURL& favicon_url)
      : favicon_url(favicon_url),
        is_bookmarked(false),
        last_visit_time(0) {}

  // The actual favicon data.
  history::FaviconBitmapResult bitmap_data[NUM_SIZES];
  // The URL this favicon was loaded from.
  const GURL favicon_url;
  // Is the favicon for a bookmarked page?
  bool is_bookmarked;
  // The last time a tab needed this favicon.
  int64 last_visit_time;
  // TODO(zea): don't keep around the actual data for locally sourced
  // favicons (UI can access those directly).

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncedFaviconInfo);
};

namespace {

// Maximum number of favicons to keep in memory (0 means no limit).
const size_t kMaxFaviconsInMem = 0;

// Maximum number of favicons to sync (0 means no limit).
// TODO(zea): pull this from a server-controlled value.
const int kMaxFaviconsInSync = 200;

// Maximum width/height resolution supported.
const int kMaxFaviconResolution = 16;

// Returns a mask of the supported favicon types.
// TODO(zea): Supporting other favicons types will involve some work in the
// favicon service and navigation controller. See crbug.com/181068.
int SupportedFaviconTypes() {
  return history::FAVICON;
}

// Returns the appropriate IconSize to use for a given gfx::Size pixel
// dimensions.
IconSize GetIconSizeBinFromBitmapResult(const gfx::Size& pixel_size) {
  int max_size =
      (pixel_size.width() > pixel_size.height() ?
       pixel_size.width() : pixel_size.height());
  if (max_size > 64)
    return SIZE_INVALID;
  else if (max_size > 32)
    return SIZE_64;
  else if (max_size > 16)
    return SIZE_32;
  else
    return SIZE_16;
}

// Helper for debug statements.
std::string IconSizeToString(IconSize icon_size) {
  switch (icon_size) {
    case SIZE_16:
      return "16";
    case SIZE_32:
      return "32";
    case SIZE_64:
      return "64";
    default:
      return "INVALID";
  }
}

}  // namespace

FaviconCacheObserver::~FaviconCacheObserver() {}

FaviconCache::FaviconCache(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      legacy_delegate_(NULL) {}

FaviconCache::~FaviconCache() {}

syncer::SyncMergeResult FaviconCache::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  return syncer::SyncMergeResult(type);
}

void FaviconCache::StopSyncing(syncer::ModelType type) {
  cancelable_task_tracker_.TryCancelAll();
  page_task_map_.clear();
}

syncer::SyncDataList FaviconCache::GetAllSyncData(syncer::ModelType type)
    const {
  return syncer::SyncDataList();
}

syncer::SyncError FaviconCache::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  return syncer::SyncError();
}

void FaviconCache::OnPageFaviconUpdated(const GURL& page_url) {
  DCHECK(page_url.is_valid());

  // If a favicon load is already happening for this url, let it finish.
  if (page_task_map_.find(page_url) != page_task_map_.end())
    return;

  DVLOG(1) << "Triggering favicon load for url " << page_url.spec();

  FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;
  // TODO(zea): This appears to only fetch one favicon (best match based on
  // desired_size_in_dip). Figure out a way to fetch all favicons we support.
  CancelableTaskTracker::TaskId id = favicon_service->GetFaviconForURL(
      FaviconService::FaviconForURLParams(
          profile_, page_url, SupportedFaviconTypes(), kMaxFaviconResolution),
      base::Bind(&FaviconCache::OnFaviconDataAvailable,
                 weak_ptr_factory_.GetWeakPtr(), page_url),
      &cancelable_task_tracker_);
  page_task_map_[page_url] = id;
}

void FaviconCache::OnFaviconVisited(const GURL& page_url,
                                    const GURL& favicon_url) {
  DCHECK(page_url.is_valid());
  if (!favicon_url.is_valid() ||
      synced_favicons_.find(favicon_url) == synced_favicons_.end()) {
    // TODO(zea): consider triggering a favicon load if we have some but not
    // all desired resolutions?
    OnPageFaviconUpdated(page_url);
    return;
  }

  DVLOG(1) << "Associating " << page_url.spec() << "with favicon at "
           << favicon_url.spec() << " and marking visited.";
  page_favicon_map_[page_url] = favicon_url;
  UpdateSyncState(favicon_url, SYNC_TRACKING);
}

bool FaviconCache::GetSyncedFaviconForFaviconURL(
    const GURL& favicon_url,
    scoped_refptr<base::RefCountedMemory>* favicon_png) const {
  if (!favicon_url.is_valid())
    return false;
  FaviconMap::const_iterator iter = synced_favicons_.find(favicon_url);

  // TODO(zea): record hit rate histogram here.
  if (iter == synced_favicons_.end())
    return false;

  // TODO(zea): support getting other resolutions.
  if (!iter->second->bitmap_data[SIZE_16].bitmap_data.get())
    return false;

  *favicon_png = iter->second->bitmap_data[SIZE_16].bitmap_data;
  return true;
}

bool FaviconCache::GetSyncedFaviconForPageURL(
    const GURL& page_url,
    scoped_refptr<base::RefCountedMemory>* favicon_png) const {
  if (!page_url.is_valid())
    return false;
  PageFaviconMap::const_iterator iter = page_favicon_map_.find(page_url);

  if (iter == page_favicon_map_.end())
    return false;

  return GetSyncedFaviconForFaviconURL(iter->second, favicon_png);
}

void FaviconCache::OnReceivedSyncFavicon(const GURL& page_url,
                                         const GURL& icon_url,
                                         const std::string& icon_bytes) {
  if (!icon_url.is_valid() || !page_url.is_valid())
    return;
  DVLOG(1) << "Associating " << page_url.spec() << " with favicon at "
           << icon_url.spec();
  page_favicon_map_[page_url] = icon_url;

  // If this favicon is already synced, do nothing else.
  if (synced_favicons_.find(icon_url) != synced_favicons_.end())
    return;

  // If there is no actual image, it means there either is no synced
  // favicon, or it's on its way (race condition).
  // TODO(zea): potentially trigger a favicon web download here (delayed?).
  if (icon_bytes.size() == 0)
    return;

  // Don't add any more favicons once we hit our in memory limit.
  // TODO(zea): UMA this.
  if (kMaxFaviconsInMem != 0 && synced_favicons_.size() > kMaxFaviconsInMem)
    return;

  SyncedFaviconInfo* favicon_info = GetFaviconInfo(icon_url);
  base::RefCountedString* temp_string = new base::RefCountedString();
  temp_string->data() = icon_bytes;
  favicon_info->bitmap_data[SIZE_16].bitmap_data = temp_string;
  // We assume legacy favicons are 16x16.
  favicon_info->bitmap_data[SIZE_16].pixel_size.set_width(16);
  favicon_info->bitmap_data[SIZE_16].pixel_size.set_width(16);

  UpdateSyncState(icon_url, SYNC_BOTH);
}

void FaviconCache::SetLegacyDelegate(FaviconCacheObserver* observer) {
  legacy_delegate_ = observer;
}

void FaviconCache::RemoveLegacyDelegate() {
  legacy_delegate_ = NULL;
}

void FaviconCache::OnFaviconDataAvailable(
    const GURL& page_url,
    const std::vector<history::FaviconBitmapResult>& bitmap_results) {
  PageTaskMap::iterator page_iter = page_task_map_.find(page_url);
  if (page_iter == page_task_map_.end())
    return;
  page_task_map_.erase(page_iter);

  if (bitmap_results.size() == 0) {
    // Either the favicon isn't loaded yet or there is no valid favicon.
    // We already cleared the task id, so just return.
    DVLOG(1) << "Favicon load failed for page " << page_url.spec();
    return;
  }

  GURL favicon_url;
  for (size_t i = 0; i < bitmap_results.size(); ++i) {
    const history::FaviconBitmapResult& bitmap_result = bitmap_results[i];
    if (!bitmap_result.icon_url.is_valid())
      continue;  // Can happen if the page is still loading.
    favicon_url = bitmap_result.icon_url;
    FaviconMap::iterator favicon_iter = synced_favicons_.find(favicon_url);

    SyncedFaviconInfo* favicon_info = NULL;
    if (favicon_iter == synced_favicons_.end()) {
      // If we've already reached our in memory favicon limit, just return.
      // TODO(zea): UMA this.
      if (kMaxFaviconsInMem != 0 &&
          synced_favicons_.size() > kMaxFaviconsInMem) {
        return;
      }
      favicon_info = new SyncedFaviconInfo(favicon_url);
      synced_favicons_[favicon_url] = make_linked_ptr(favicon_info);
    } else {
      // TODO(zea): add a dampening factor so animated favicons don't constantly
      // update the image data.
      favicon_info = favicon_iter->second.get();
    }
    favicon_info->last_visit_time = std::max(
        favicon_info->last_visit_time,
        syncer::TimeToProtoTime(base::Time::Now()));

    if (!bitmap_result.is_valid()) {
      DVLOG(1) << "Received invalid favicon at " << favicon_url.spec();
      continue;
    }

    IconSize icon_size = GetIconSizeBinFromBitmapResult(
        bitmap_result.pixel_size);
    if (icon_size == SIZE_INVALID) {
      DVLOG(1) << "Ignoring unsupported resolution "
               << bitmap_result.pixel_size.height() << "x"
               << bitmap_result.pixel_size.width();
    } else {
      DVLOG(1) << "Storing " << IconSizeToString(icon_size) << "p"
               << " favicon for " << favicon_info->favicon_url.spec()
               << " with size " << bitmap_result.bitmap_data->size()
               << " bytes.";
      favicon_info->bitmap_data[icon_size] = bitmap_result;

    }
  }
  if (!favicon_url.is_valid())
    return;
  page_favicon_map_[page_url] = favicon_url;
  UpdateSyncState(favicon_url, SYNC_BOTH);

  if (legacy_delegate_)
    legacy_delegate_->OnFaviconUpdated(page_url, favicon_url);
}

void FaviconCache::UpdateSyncState(const GURL& icon_url,
                                   SyncState state_to_update) {
  // TODO(zea): implement this.
}

SyncedFaviconInfo* FaviconCache::GetFaviconInfo(
    const GURL& icon_url) {
  if (synced_favicons_.count(icon_url) != 0)
    return synced_favicons_[icon_url].get();

  // TODO(zea): implement in-memory eviction.
  SyncedFaviconInfo* favicon_info = new SyncedFaviconInfo(icon_url);
  synced_favicons_[icon_url] = make_linked_ptr(favicon_info);
  return favicon_info;
}

size_t FaviconCache::NumFaviconsForTest() const {
  return synced_favicons_.size();
}

}  // namespace browser_sync
