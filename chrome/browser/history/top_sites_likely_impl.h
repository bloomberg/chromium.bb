// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_LIKELY_IMPL_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_LIKELY_IMPL_H_

#include <list>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/top_sites_backend.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/thumbnail_score.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"

class Profile;

namespace base {
class FilePath;
class RefCountedBytes;
class RefCountedMemory;
}

namespace history {

class TopSitesCache;
class TopSitesLikelyImplTest;


// This class allows requests for most visited urls and thumbnails on any
// thread. All other methods must be invoked on the UI thread. All mutations
// to internal state happen on the UI thread and are scheduled to update the
// db using TopSitesBackend.
class TopSitesLikelyImpl : public TopSites {
 public:
  explicit TopSitesLikelyImpl(Profile* profile);

  // Initializes TopSitesLikelyImpl.
  void Init(const base::FilePath& db_name);

  virtual bool SetPageThumbnail(const GURL& url,
                        const gfx::Image& thumbnail,
                        const ThumbnailScore& score) OVERRIDE;
  virtual void GetMostVisitedURLs(
      const GetMostVisitedURLsCallback& callback) OVERRIDE;
  virtual bool GetPageThumbnail(
      const GURL& url, scoped_refptr<base::RefCountedMemory>* bytes) OVERRIDE;
  virtual bool GetPageThumbnailScore(const GURL& url,
                                     ThumbnailScore* score) OVERRIDE;
  virtual bool GetTemporaryPageThumbnailScore(const GURL& url,
                                              ThumbnailScore* score) OVERRIDE;
  virtual void MigrateFromHistory() OVERRIDE;
  virtual void FinishHistoryMigration(const ThumbnailMigration& data) OVERRIDE;
  virtual void HistoryLoaded() OVERRIDE;
  virtual void SyncWithHistory() OVERRIDE;
  virtual bool HasBlacklistedItems() const OVERRIDE;
  virtual void AddBlacklistedURL(const GURL& url) OVERRIDE;
  virtual void RemoveBlacklistedURL(const GURL& url) OVERRIDE;
  virtual bool IsBlacklisted(const GURL& url) OVERRIDE;
  virtual void ClearBlacklistedURLs() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual CancelableRequestProvider::Handle StartQueryForMostVisited() OVERRIDE;
  virtual bool IsKnownURL(const GURL& url) OVERRIDE;
  virtual bool IsFull() OVERRIDE;
  virtual MostVisitedURLList GetPrepopulatePages() OVERRIDE;
  virtual bool loaded() const OVERRIDE;

 protected:
  virtual ~TopSitesLikelyImpl();

 private:
  friend class TopSitesLikelyImplTest;
  FRIEND_TEST_ALL_PREFIXES(TopSitesLikelyImplTest, DiffMostVisited);

  typedef std::pair<GURL, Images> TempImage;
  typedef std::list<TempImage> TempImages;

  // Generates the diff of things that happened between "old" and "new."
  //
  // The URLs that are in "new" but not "old" will be have their index into
  // "new" put in |added_urls|. The URLs that are in "old" but not "new" will
  // have their index into "old" put into |deleted_urls|.
  //
  // URLs appearing in both old and new lists but having different indices will
  // have their index into "new" be put into |moved_urls|.
  static void DiffMostVisited(const MostVisitedURLList& old_list,
                              const MostVisitedURLList& new_list,
                              TopSitesDelta* delta);

  // Enumeration of the possible states history can be in. These values do not
  // necessarily reflect the loaded state of history. In particular if the
  // history backend is unloaded |history_state_| may be HISTORY_LOADED.
  enum HistoryLoadState {
    // We're waiting for history to finish loading.
    HISTORY_LOADING,

    // History finished loading and we need to migrate top sites out of history.
    HISTORY_MIGRATING,

    // History is loaded.
    HISTORY_LOADED
  };

  // Enumeration of possible states the top sites backend can be in.
  enum TopSitesLoadState {
    // We're waiting for the backend to finish loading.
    TOP_SITES_LOADING,

    // The backend finished loading, but we may need to migrate. This is true if
    // the top sites db didn't exist, or if the db existed but is from an old
    // version.
    TOP_SITES_LOADED_WAITING_FOR_HISTORY,

    // Top sites is loaded.
    TOP_SITES_LOADED
  };

  // Sets the thumbnail without writing to the database. Useful when
  // reading last known top sites from the DB.
  // Returns true if the thumbnail was set, false if the existing one is better.
  bool SetPageThumbnailNoDB(const GURL& url,
                            const base::RefCountedBytes* thumbnail_data,
                            const ThumbnailScore& score);

  // A version of SetPageThumbnail that takes RefCountedBytes as
  // returned by HistoryService.
  bool SetPageThumbnailEncoded(const GURL& url,
                               const base::RefCountedBytes* thumbnail,
                               const ThumbnailScore& score);

  // Encodes the bitmap to bytes for storage to the db. Returns true if the
  // bitmap was successfully encoded.
  static bool EncodeBitmap(const gfx::Image& bitmap,
                           scoped_refptr<base::RefCountedBytes>* bytes);

  // Removes the cached thumbnail for url. Does nothing if |url| if not cached
  // in |temp_images_|.
  void RemoveTemporaryThumbnailByURL(const GURL& url);

  // Add a thumbnail for an unknown url. See temp_thumbnails_map_.
  void AddTemporaryThumbnail(const GURL& url,
                             const base::RefCountedBytes* thumbnail,
                             const ThumbnailScore& score);

  // Called by our timer. Starts the query for the most visited sites.
  void TimerFired();

  // Finds the given URL in the redirect chain for the given TopSite, and
  // returns the distance from the destination in hops that the given URL is.
  // The URL is assumed to be in the list. The destination is 0.
  static int GetRedirectDistanceForURL(const MostVisitedURL& most_visited,
                                       const GURL& url);

  // Add prepopulated pages: 'welcome to Chrome' and themes gallery to |urls|.
  // Returns true if any pages were added.
  bool AddPrepopulatedPages(MostVisitedURLList* urls);

  // Takes |urls|, produces it's copy in |out| after removing blacklisted URLs.
  void ApplyBlacklist(const MostVisitedURLList& urls, MostVisitedURLList* out);

  // Converts a url into a canonical string representation.
  std::string GetURLString(const GURL& url);

  // Returns an MD5 hash of the URL. Hashing is required for blacklisted URLs.
  std::string GetURLHash(const GURL& url);

  // Returns the delay until the next update of history is needed.
  // Uses num_urls_changed
  base::TimeDelta GetUpdateDelay();

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Resets top_sites_ and updates the db (in the background). All mutations to
  // top_sites_ *must* go through this. Should be called from the UI thread.
  void SetTopSites(const MostVisitedURLList& new_top_sites);

  // Returns the number of most visted results to request from history. This
  // changes depending upon how many urls have been blacklisted. Should be
  // called from the UI thread.
  int num_results_to_request_from_history() const;

  // Invoked when transitioning to LOADED. Notifies any queued up callbacks.
  // Should be called from the UI thread.
  void MoveStateToLoaded();

  void ResetThreadSafeCache();

  void ResetThreadSafeImageCache();

  void NotifyTopSitesChanged();

  // Stops and starts timer with a delay of |delta|.
  void RestartQueryForTopSitesTimer(base::TimeDelta delta);

  // Callback after TopSitesBackend has finished migration. This tells history
  // to finish it's side of migration (nuking thumbnails on disk). Should be
  // called from the UI thread.
  void OnHistoryMigrationWrittenToDisk();

  // Callback from TopSites with the top sites/thumbnails. Should be called
  // from the UI thread.
  void OnGotMostVisitedThumbnails(
      const scoped_refptr<MostVisitedThumbnails>& thumbnails,
      const bool* need_history_migration);

  // Called when history service returns a list of top URLs.
  void OnTopSitesAvailableFromHistory(CancelableRequestProvider::Handle handle,
                                      MostVisitedURLList data);

  scoped_refptr<TopSitesBackend> backend_;

  // The top sites data.
  scoped_ptr<TopSitesCache> cache_;

  // Copy of the top sites data that may be accessed on any thread (assuming
  // you hold |lock_|). The data in |thread_safe_cache_| has blacklisted and
  // pinned urls applied (|cache_| does not).
  scoped_ptr<TopSitesCache> thread_safe_cache_;

  Profile* profile_;

  // Lock used to access |thread_safe_cache_|.
  mutable base::Lock lock_;

  // Need a separate consumer for each CancelableRequestProvider we interact
  // with (HistoryService and TopSitesBackend).
  CancelableRequestConsumer history_consumer_;
  CancelableTaskTracker cancelable_task_tracker_;

  // Timer that asks history for the top sites. This is used to make sure our
  // data stays in sync with history.
  base::OneShotTimer<TopSitesLikelyImpl> timer_;

  // The time we started |timer_| at. Only valid if |timer_| is running.
  base::TimeTicks timer_start_time_;

  content::NotificationRegistrar registrar_;

  // The number of URLs changed on the last update.
  size_t last_num_urls_changed_;

  // The pending requests for the top sites list. Can only be non-empty at
  // startup. After we read the top sites from the DB, we'll always have a
  // cached list and be able to run callbacks immediately.
  PendingCallbacks pending_callbacks_;

  // Stores thumbnails for unknown pages. When SetPageThumbnail is
  // called, if we don't know about that URL yet and we don't have
  // enough Top Sites (new profile), we store it until the next
  // SetTopSites call.
  TempImages temp_images_;

  // See description above HistoryLoadState.
  HistoryLoadState history_state_;

  // See description above TopSitesLoadState.
  TopSitesLoadState top_sites_state_;

  // URL List of prepopulated page.
  std::vector<GURL> prepopulated_page_urls_;

  // Are we loaded?
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesLikelyImpl);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_LIKELY_IMPL_H_
