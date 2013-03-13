// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_FAVICON_CACHE_H_
#define CHROME_BROWSER_SYNC_GLUE_FAVICON_CACHE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "googleurl/src/gurl.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/syncable_service.h"

class Profile;

namespace history {
struct FaviconBitmapResult;
}

namespace browser_sync {

enum IconSize {
  SIZE_INVALID,
  SIZE_16,
  SIZE_32,
  SIZE_64,
  NUM_SIZES
};

struct SyncedFaviconInfo;

// Observer interface.
class FaviconCacheObserver {
 public:
  virtual void OnFaviconUpdated(const GURL& page_url, const GURL& icon_url) = 0;

 protected:
  virtual ~FaviconCacheObserver();
};

// Encapsulates the logic for loading and storing synced favicons.
// TODO(zea): make this a ProfileKeyedService.
class FaviconCache : public syncer::SyncableService {
 public:
  explicit FaviconCache(Profile* profile);
  virtual ~FaviconCache();

  // SyncableService implementation.
  // TODO(zea): implement these.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
      const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // If a valid favicon for the icon at |favicon_url| is found, fills
  // |favicon_png| with the png-encoded image and returns true. Else, returns
  // false.
  bool GetSyncedFaviconForFaviconURL(
      const GURL& favicon_url,
      scoped_refptr<base::RefCountedMemory>* favicon_png) const;

  // If a valid favicon for the icon associated with |page_url| is found, fills
  // |favicon_png| with the png-encoded image and returns true. Else, returns
  // false.
  bool GetSyncedFaviconForPageURL(
      const GURL& page_url,
      scoped_refptr<base::RefCountedMemory>* favicon_png) const;

  // Load the favicon for |page_url|. Will create a new sync node or update
  // an existing one as necessary, and set the last visit time to the current
  // time. Only those favicon types defined in SupportedFaviconTypes will be
  // synced.
  void OnPageFaviconUpdated(const GURL& page_url);

  // Update the visit count for the favicon associated with |favicon_url|.
  // If no favicon exists associated with |favicon_url|, triggers a load
  // for the favicon associated with |page_url|.
  void OnFaviconVisited(const GURL& page_url, const GURL& favicon_url);

  // Consume Session sync favicon data. Will not overwrite existing favicons.
  // If |icon_bytes| is empty, only updates the page->favicon url mapping.
  void OnReceivedSyncFavicon(const GURL& page_url,
                             const GURL& icon_url,
                             const std::string& icon_bytes);

  // Support for syncing favicons using the legacy format (within tab sync).
  void SetLegacyDelegate(FaviconCacheObserver* observer);
  void RemoveLegacyDelegate();

 private:
  friend class SyncFaviconCacheTest;

  // Map of favicon url to favicon image.
  typedef std::map<GURL, linked_ptr<SyncedFaviconInfo> > FaviconMap;
  // Map of page url to task id (for favicon loading).
  typedef std::map<GURL, CancelableTaskTracker::TaskId> PageTaskMap;
  // Map of page url to favicon url.
  typedef std::map<GURL, GURL> PageFaviconMap;

  // Enum used to decide which sync datatypes to update on a favicon change.
  enum SyncState {
    SYNC_IMAGE,
    SYNC_TRACKING,
    SYNC_BOTH
  };

  // Callback method to store a tab's favicon into its sync node once it becomes
  // available. Does nothing if no favicon data was available.
  void OnFaviconDataAvailable(
      const GURL& favicon_url,
      const std::vector<history::FaviconBitmapResult>& bitmap_result);

  // Helper method to update the sync state of the favicon at |icon_url|.
  void UpdateSyncState(const GURL& icon_url, SyncState state_to_update);

  // Helper method to get favicon info from |synced_favicons_|. If no info
  // exists for |icon_url|, creates a new SyncedFaviconInfo and returns it.
  SyncedFaviconInfo* GetFaviconInfo(const GURL& icon_url);

  // For testing only.
  size_t NumFaviconsForTest() const;

  // Trask tracker for loading favicons.
  CancelableTaskTracker cancelable_task_tracker_;

  // Our actual cached favicon data.
  FaviconMap synced_favicons_;

  // Our set of pending favicon loads, indexed by page url.
  PageTaskMap page_task_map_;

  // Map of page and associated favicon urls.
  PageFaviconMap page_favicon_map_;

  Profile* profile_;

  // TODO(zea): consider creating a favicon handler here for fetching unsynced
  // favicons from the web.

  base::WeakPtrFactory<FaviconCache> weak_ptr_factory_;

  // Delegate for legacy favicon sync support.
  // TODO(zea): Remove this eventually.
  FaviconCacheObserver* legacy_delegate_;

  DISALLOW_COPY_AND_ASSIGN(FaviconCache);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_FAVICON_CACHE_H_
