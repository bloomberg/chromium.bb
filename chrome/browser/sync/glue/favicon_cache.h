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
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "sync/api/sync_change.h"
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
class FaviconCache : public syncer::SyncableService,
                     public content::NotificationObserver {
 public:
  FaviconCache(Profile* profile, int max_sync_favicon_limit);
  virtual ~FaviconCache();

  // SyncableService implementation.
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
  // Safe to call within a transaction.
  void OnReceivedSyncFavicon(const GURL& page_url,
                             const GURL& icon_url,
                             const std::string& icon_bytes,
                             int64 visit_time_ms);

  // Support for syncing favicons using the legacy format (within tab sync).
  void SetLegacyDelegate(FaviconCacheObserver* observer);
  void RemoveLegacyDelegate();

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class SyncFaviconCacheTest;

  // Functor for ordering SyncedFaviconInfo objects by recency;
  struct FaviconRecencyFunctor {
    bool operator()(const linked_ptr<SyncedFaviconInfo>& lhs,
                    const linked_ptr<SyncedFaviconInfo>& rhs) const;
  };


  // Map of favicon url to favicon image.
  typedef std::map<GURL, linked_ptr<SyncedFaviconInfo> > FaviconMap;
  typedef std::set<linked_ptr<SyncedFaviconInfo>,
                   FaviconRecencyFunctor> RecencySet;
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
      const GURL& page_url,
      const std::vector<history::FaviconBitmapResult>& bitmap_result);

  // Helper method to update the sync state of the favicon at |icon_url|.
  // Note: should only be called after both FAVICON_IMAGES and FAVICON_TRACKING
  // have been successfully set up.
  void UpdateSyncState(const GURL& icon_url,
                       SyncState state_to_update,
                       syncer::SyncChange::SyncChangeType change_type);

  // Helper method to get favicon info from |synced_favicons_|. If no info
  // exists for |icon_url|, creates a new SyncedFaviconInfo in both
  // |synced_favicons_| and |recent_favicons_| and returns it.
  SyncedFaviconInfo* GetFaviconInfo(const GURL& icon_url);

  // Updates the last visit time for the favicon at |icon_url| to |time| (and
  // correspondly updates position in |recent_favicons_|.
  void UpdateFaviconVisitTime(const GURL& icon_url, base::Time time);

  // Expiration method. Looks through |recent_favicons_| to find any favicons
  // that should be expired in order to maintain the sync favicon limit,
  // appending deletions to |image_changes| and |tracking_changes| as necessary.
  void ExpireFaviconsIfNecessary(syncer::SyncChangeList* image_changes,
                                 syncer::SyncChangeList* tracking_changes);

  // Returns the local favicon url associated with |sync_favicon| if one exists
  // in |synced_favicons_|, else returns an invalid GURL.
  GURL GetLocalFaviconFromSyncedData(
      const syncer::SyncData& sync_favicon) const;

  // Merges |sync_favicon| into |synced_favicons_|, updating |local_changes|
  // with any changes that should be pushed to the sync processor.
  void MergeSyncFavicon(const syncer::SyncData& sync_favicon,
                        syncer::SyncChangeList* sync_changes);

  // Updates |synced_favicons_| with the favicon data from |sync_favicon|.
  void AddLocalFaviconFromSyncedData(const syncer::SyncData& sync_favicon);

  // Creates a SyncData object from the |type| data of |favicon_url|
  // from within |synced_favicons_|.
  syncer::SyncData CreateSyncDataFromLocalFavicon(
      syncer::ModelType type,
      const GURL& favicon_url) const;

  // Deletes all synced favicons corresponding with |favicon_urls| and pushes
  // the deletions to sync.
  void DeleteSyncedFavicons(const std::set<GURL>& favicon_urls);

  // Deletes the favicon pointed to by |favicon_iter| and appends the necessary
  // sync deletions to |image_changes| and |tracking_changes|.
  // Returns an iterator to the favicon after |favicon_iter|.
  FaviconMap::iterator DeleteSyncedFavicon(
      FaviconMap::iterator favicon_iter,
      syncer::SyncChangeList* image_changes,
      syncer::SyncChangeList* tracking_changes);

  // Locally drops the favicon pointed to by |favicon_iter|.
  void DropSyncedFavicon(FaviconMap::iterator favicon_iter);

  // For testing only.
  size_t NumFaviconsForTest() const;
  size_t NumTasksForTest() const;

  // Trask tracker for loading favicons.
  CancelableTaskTracker cancelable_task_tracker_;

  // Our actual cached favicon data.
  FaviconMap synced_favicons_;

  // An LRU ordering of the favicons comprising |synced_favicons_| (oldest to
  // newest).
  RecencySet recent_favicons_;

  // Our set of pending favicon loads, indexed by page url.
  PageTaskMap page_task_map_;

  // Map of page and associated favicon urls.
  PageFaviconMap page_favicon_map_;

  Profile* profile_;

  // TODO(zea): consider creating a favicon handler here for fetching unsynced
  // favicons from the web.

  // Weak pointer factory for favicon loads.
  base::WeakPtrFactory<FaviconCache> weak_ptr_factory_;

  // Delegate for legacy favicon sync support.
  // TODO(zea): Remove this eventually.
  FaviconCacheObserver* legacy_delegate_;

  scoped_ptr<syncer::SyncChangeProcessor> favicon_images_sync_processor_;
  scoped_ptr<syncer::SyncChangeProcessor> favicon_tracking_sync_processor_;

  // For listening to history deletions.
  content::NotificationRegistrar notification_registrar_;

  // Maximum number of favicons to sync. 0 means no limit.
  const size_t max_sync_favicon_limit_;

  DISALLOW_COPY_AND_ASSIGN(FaviconCache);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_FAVICON_CACHE_H_
