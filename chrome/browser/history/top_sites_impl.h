// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_IMPL_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_IMPL_H_

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
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/top_sites_backend.h"
#include "components/history/core/common/thumbnail_score.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class Profile;

namespace base {
class FilePath;
class RefCountedBytes;
class RefCountedMemory;
}

namespace history {

class TopSitesCache;
class TopSitesImplTest;

// This class allows requests for most visited urls and thumbnails on any
// thread. All other methods must be invoked on the UI thread. All mutations
// to internal state happen on the UI thread and are scheduled to update the
// db using TopSitesBackend.
class TopSitesImpl : public TopSites {
 public:
  explicit TopSitesImpl(Profile* profile);

  // Initializes TopSitesImpl.
  void Init(const base::FilePath& db_name);

  virtual bool SetPageThumbnail(const GURL& url,
                                const gfx::Image& thumbnail,
                                const ThumbnailScore& score) OVERRIDE;
  virtual bool SetPageThumbnailToJPEGBytes(
      const GURL& url,
      const base::RefCountedMemory* memory,
      const ThumbnailScore& score) OVERRIDE;
  virtual void GetMostVisitedURLs(
      const GetMostVisitedURLsCallback& callback,
      bool include_forced_urls) OVERRIDE;
  virtual bool GetPageThumbnail(
      const GURL& url,
      bool prefix_match,
      scoped_refptr<base::RefCountedMemory>* bytes) OVERRIDE;
  virtual bool GetPageThumbnailScore(const GURL& url,
                                     ThumbnailScore* score) OVERRIDE;
  virtual bool GetTemporaryPageThumbnailScore(const GURL& url,
                                              ThumbnailScore* score) OVERRIDE;
  virtual void SyncWithHistory() OVERRIDE;
  virtual bool HasBlacklistedItems() const OVERRIDE;
  virtual void AddBlacklistedURL(const GURL& url) OVERRIDE;
  virtual void RemoveBlacklistedURL(const GURL& url) OVERRIDE;
  virtual bool IsBlacklisted(const GURL& url) OVERRIDE;
  virtual void ClearBlacklistedURLs() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual CancelableRequestProvider::Handle StartQueryForMostVisited() OVERRIDE;
  virtual bool IsKnownURL(const GURL& url) OVERRIDE;
  virtual const std::string& GetCanonicalURLString(
      const GURL& url) const OVERRIDE;
  virtual bool IsNonForcedFull() OVERRIDE;
  virtual bool IsForcedFull() OVERRIDE;
  virtual MostVisitedURLList GetPrepopulatePages() OVERRIDE;
  virtual bool loaded() const OVERRIDE;
  virtual bool AddForcedURL(const GURL& url, const base::Time& time) OVERRIDE;

 protected:
  virtual ~TopSitesImpl();

 private:
  friend class TopSitesImplTest;
  FRIEND_TEST_ALL_PREFIXES(TopSitesImplTest, DiffMostVisited);
  FRIEND_TEST_ALL_PREFIXES(TopSitesImplTest, DiffMostVisitedWithForced);

  typedef base::Callback<void(const MostVisitedURLList&,
                              const MostVisitedURLList&)> PendingCallback;

  typedef std::pair<GURL, Images> TempImage;
  typedef std::list<TempImage> TempImages;
  typedef std::vector<PendingCallback> PendingCallbacks;

  // Generates the diff of things that happened between "old" and "new."
  //
  // This treats forced URLs separately than non-forced URLs.
  //
  // The URLs that are in "new" but not "old" will be have their index into
  // "new" put in |added_urls|. The non-forced URLs that are in "old" but not
  // "new" will have their index into "old" put into |deleted_urls|.
  //
  // URLs appearing in both old and new lists but having different indices will
  // have their index into "new" be put into |moved_urls|.
  static void DiffMostVisited(const MostVisitedURLList& old_list,
                              const MostVisitedURLList& new_list,
                              TopSitesDelta* delta);

  // Sets the thumbnail without writing to the database. Useful when
  // reading last known top sites from the DB.
  // Returns true if the thumbnail was set, false if the existing one is better.
  bool SetPageThumbnailNoDB(const GURL& url,
                            const base::RefCountedMemory* thumbnail_data,
                            const ThumbnailScore& score);

  // A version of SetPageThumbnail that takes RefCountedBytes as
  // returned by HistoryService.
  bool SetPageThumbnailEncoded(const GURL& url,
                               const base::RefCountedMemory* thumbnail,
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
                             const base::RefCountedMemory* thumbnail,
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
  bool AddPrepopulatedPages(MostVisitedURLList* urls,
                            size_t num_forced_urls);

  // Add all the forced URLs from |cache_| into |new_list|, making sure not to
  // add any URL that's already in |new_list|'s non-forced URLs. The forced URLs
  // in |cache_| and |new_list| are assumed to appear at the front of the list
  // and be sorted in increasing |last_forced_time|. This will still be true
  // after the call. If the list of forced URLs overflows the older ones are
  // dropped. Returns the number of forced URLs after the merge.
  size_t MergeCachedForcedURLs(MostVisitedURLList* new_list);

  // Takes |urls|, produces it's copy in |out| after removing blacklisted URLs.
  // Also ensures we respect the maximum number of forced URLs and non-forced
  // URLs.
  void ApplyBlacklist(const MostVisitedURLList& urls, MostVisitedURLList* out);

  // Returns an MD5 hash of the URL. Hashing is required for blacklisted URLs.
  std::string GetURLHash(const GURL& url);

  // Returns the delay until the next update of history is needed.
  // Uses num_urls_changed
  base::TimeDelta GetUpdateDelay();

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Updates URLs in |cache_| and the db (in the background).
  // The non-forced URLs in |new_top_sites| replace those in |cache_|.
  // The forced URLs of |new_top_sites| are merged with those in |cache_|,
  // if the list of forced URLs overflows, the oldest ones are dropped.
  // All mutations to cache_ *must* go through this. Should
  // be called from the UI thread.
  void SetTopSites(const MostVisitedURLList& new_top_sites);

  // Returns the number of most visited results to request from history. This
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

  // Callback from TopSites with the top sites/thumbnails. Should be called
  // from the UI thread.
  void OnGotMostVisitedThumbnails(
      const scoped_refptr<MostVisitedThumbnails>& thumbnails);

  // Called when history service returns a list of top URLs.
  void OnTopSitesAvailableFromHistory(const MostVisitedURLList* data);

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
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Timer that asks history for the top sites. This is used to make sure our
  // data stays in sync with history.
  base::OneShotTimer<TopSitesImpl> timer_;

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
  // SetNonForcedTopSites call.
  TempImages temp_images_;

  // URL List of prepopulated page.
  std::vector<GURL> prepopulated_page_urls_;

  // Are we loaded?
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesImpl);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_IMPL_H_
