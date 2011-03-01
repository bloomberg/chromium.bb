// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_TOP_SITES_H_
#define CHROME_BROWSER_HISTORY_TOP_SITES_H_
#pragma once

#include <list>
#include <set>
#include <string>
#include <utility>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/ref_counted.h"
#include "base/ref_counted_memory.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "base/timer.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/page_usage_data.h"
#include "chrome/common/thumbnail_score.h"
#include "content/browser/cancelable_request.h"
#include "googleurl/src/gurl.h"

class DictionaryValue;
class FilePath;
class SkBitmap;
class Profile;

namespace history {

class TopSitesCache;
class TopSitesBackend;
class TopSitesTest;

// Stores the data for the top "most visited" sites. This includes a cache of
// the most visited data from history, as well as the corresponding thumbnails
// of those sites.
//
// This class allows requests for most visited urls and thumbnails on any
// thread. All other methods must be invoked on the UI thread. All mutations
// to internal state happen on the UI thread and are scheduled to update the
// db using TopSitesBackend.
class TopSites
    : public base::RefCountedThreadSafe<TopSites>,
      public NotificationObserver,
      public CancelableRequestProvider {
 public:
  explicit TopSites(Profile* profile);

  // Initializes TopSites.
  void Init(const FilePath& db_name);

  // Sets the given thumbnail for the given URL. Returns true if the thumbnail
  // was updated. False means either the URL wasn't known to us, or we felt
  // that our current thumbnail was superior to the given one.
  bool SetPageThumbnail(const GURL& url,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score);

  // Callback for GetMostVisitedURLs.
  typedef Callback1<const MostVisitedURLList&>::Type GetTopSitesCallback;
  typedef std::set<scoped_refptr<CancelableRequest<GetTopSitesCallback> > >
      PendingCallbackSet;

  // Returns a list of most visited URLs via a callback.
  // This may be invoked on any thread.
  // NOTE: the callback may be called immediately if we have the data cached.
  void GetMostVisitedURLs(CancelableRequestConsumer* consumer,
                          GetTopSitesCallback* callback);

  // Get a thumbnail for a given page. Returns true iff we have the thumbnail.
  // This may be invoked on any thread.
  // As this method may be invoked on any thread the ref count needs to be
  // upped before this method returns, so this takes a scoped_refptr*.
  bool GetPageThumbnail(const GURL& url,
                        scoped_refptr<RefCountedBytes>* bytes);

  // Get a thumbnail score for a given page. Returns true iff we have the
  // thumbnail score.  This may be invoked on any thread. The score will
  // be copied to |score|.
  virtual bool GetPageThumbnailScore(const GURL& url, ThumbnailScore* score);

  // Invoked from History if migration is needed. If this is invoked it will
  // be before HistoryLoaded is invoked.
  void MigrateFromHistory();

  // Invoked with data from migrating thumbnails out of history.
  void FinishHistoryMigration(const ThumbnailMigration& data);

  // Invoked from history when it finishes loading. If MigrateFromHistory was
  // not invoked at this point then we load from the top sites service.
  void HistoryLoaded();

  // Blacklisted URLs

  // Returns true if there is at least one item in the blacklist.
  bool HasBlacklistedItems() const;

  // Add a  URL to the blacklist.
  void AddBlacklistedURL(const GURL& url);

  // Removes a URL from the blacklist.
  void RemoveBlacklistedURL(const GURL& url);

  // Returns true if the URL is blacklisted.
  bool IsBlacklisted(const GURL& url);

  // Clear the blacklist.
  void ClearBlacklistedURLs();

  // Pinned URLs

  // Pin a URL at |index|.
  void AddPinnedURL(const GURL& url, size_t index);

  // Returns true if a URL is pinned.
  bool IsURLPinned(const GURL& url);

  // Unpin a URL.
  void RemovePinnedURL(const GURL& url);

  // Return a URL pinned at |index| via |out|. Returns true if there
  // is a URL pinned at |index|.
  bool GetPinnedURLAtIndex(size_t index, GURL* out);

  // Shuts down top sites.
  void Shutdown();

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

  // Query history service for the list of available thumbnails. Returns the
  // handle for the request, or NULL if a request could not be made.
  // Public only for testing purposes.
  CancelableRequestProvider::Handle StartQueryForMostVisited();

  bool loaded() const { return loaded_; }

  // Returns true if the given URL is known to the top sites service.
  // This function also returns false if TopSites isn't loaded yet.
  virtual bool IsKnownURL(const GURL& url);

  // Returns true if the top sites list is full (i.e. we already have the
  // maximum number of top sites).  This function also returns false if
  // TopSites isn't loaded yet.
  virtual bool IsFull();

 protected:
  // For allowing inheritance.
  virtual ~TopSites();

 private:
  friend class base::RefCountedThreadSafe<TopSites>;
  friend class TopSitesTest;

  typedef std::pair<GURL, Images> TempImage;
  typedef std::list<TempImage> TempImages;

  // Enumeration of the possible states history can be in.
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
                            const RefCountedBytes* thumbnail_data,
                            const ThumbnailScore& score);

  // A version of SetPageThumbnail that takes RefCountedBytes as
  // returned by HistoryService.
  bool SetPageThumbnailEncoded(const GURL& url,
                               const RefCountedBytes* thumbnail,
                               const ThumbnailScore& score);

  // Encodes the bitmap to bytes for storage to the db. Returns true if the
  // bitmap was successfully encoded.
  static bool EncodeBitmap(const SkBitmap& bitmap,
                           scoped_refptr<RefCountedBytes>* bytes);

  // Removes the cached thumbnail for url. Does nothing if |url| if not cached
  // in |temp_images_|.
  void RemoveTemporaryThumbnailByURL(const GURL& url);

  // Add a thumbnail for an unknown url. See temp_thumbnails_map_.
  void AddTemporaryThumbnail(const GURL& url,
                             const RefCountedBytes* thumbnail,
                             const ThumbnailScore& score);

  // Called by our timer. Starts the query for the most visited sites.
  void TimerFired();

  // Finds the given URL in the redirect chain for the given TopSite, and
  // returns the distance from the destination in hops that the given URL is.
  // The URL is assumed to be in the list. The destination is 0.
  static int GetRedirectDistanceForURL(const MostVisitedURL& most_visited,
                                       const GURL& url);

  // Returns the set of prepopulate pages.
  static MostVisitedURLList GetPrepopulatePages();

  // Add prepopulated pages: 'welcome to Chrome' and themes gallery to |urls|.
  // Returns true if any pages were added.
  static bool AddPrepopulatedPages(MostVisitedURLList* urls);

  // Convert pinned_urls_ dictionary to the new format. Use URLs as
  // dictionary keys.
  void MigratePinnedURLs();

  // Takes |urls|, produces it's copy in |out| after removing
  // blacklisted URLs and reordering pinned URLs.
  void ApplyBlacklistAndPinnedURLs(const MostVisitedURLList& urls,
                                   MostVisitedURLList* out);

  // Converts a url into a canonical string representation.
  std::string GetURLString(const GURL& url);

  // Returns an MD5 hash of the URL. Hashing is required for blacklisted URLs.
  std::string GetURLHash(const GURL& url);

  // Returns the delay until the next update of history is needed.
  // Uses num_urls_changed
  base::TimeDelta GetUpdateDelay();

  // Executes all of the callbacks in |pending_callbacks|. This is used after
  // we finish loading if any requests came in before we loaded.
  static void ProcessPendingCallbacks(
      const PendingCallbackSet& pending_callbacks,
      const MostVisitedURLList& urls);

  // Implementation of NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Resets top_sites_ and updates the db (in the background). All mutations to
  // top_sites_ *must* go through this.
  void SetTopSites(const MostVisitedURLList& new_top_sites);

  // Returns the number of most visted results to request from history. This
  // changes depending upon how many urls have been blacklisted.
  int num_results_to_request_from_history() const;

  // Invoked when transitioning to LOADED. Notifies any queued up callbacks.
  void MoveStateToLoaded();

  void ResetThreadSafeCache();

  void ResetThreadSafeImageCache();

  // Stops and starts timer with a delay of |delta|.
  void RestartQueryForTopSitesTimer(base::TimeDelta delta);

  // Callback after TopSitesBackend has finished migration. This tells history
  // to finish it's side of migration (nuking thumbnails on disk).
  void OnHistoryMigrationWrittenToDisk(
      CancelableRequestProvider::Handle handle);

  // Callback from TopSites with the top sites/thumbnails.
  void OnGotMostVisitedThumbnails(CancelableRequestProvider::Handle handle,
                                  scoped_refptr<MostVisitedThumbnails> data,
                                  bool may_need_history_migration);

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

  CancelableRequestConsumer cancelable_consumer_;

  // Timer that asks history for the top sites. This is used to make sure our
  // data stays in sync with history.
  base::OneShotTimer<TopSites> timer_;

  // The time we started |timer_| at. Only valid if |timer_| is running.
  base::TimeTicks timer_start_time_;

  NotificationRegistrar registrar_;

  // The number of URLs changed on the last update.
  size_t last_num_urls_changed_;

  // The map of requests for the top sites list. Can only be
  // non-empty at startup. After we read the top sites from the DB, we'll
  // always have a cached list.
  PendingCallbackSet pending_callbacks_;

  // Stores thumbnails for unknown pages. When SetPageThumbnail is
  // called, if we don't know about that URL yet and we don't have
  // enough Top Sites (new profile), we store it until the next
  // SetTopSites call.
  TempImages temp_images_;

  // Blacklisted and pinned URLs are stored in Preferences.

  // Blacklisted URLs. They are filtered out from the list of Top
  // Sites when GetMostVisitedURLs is called. Note that we are still
  // storing all URLs, but filtering on access. It is a dictionary,
  // key is the URL, value is a dummy value. This is owned by the
  // PrefService.
  DictionaryValue* blacklist_;

  // This is a dictionary for the pinned URLs for the the most visited part of
  // the new tab page. Key is the URL, value is index where it is pinned at (may
  // be the same as key). This is owned by the PrefService.
  DictionaryValue* pinned_urls_;

  // See description above HistoryLoadState.
  HistoryLoadState history_state_;

  // See description above TopSitesLoadState.
  TopSitesLoadState top_sites_state_;

  // Are we loaded?
  bool loaded_;

  DISALLOW_COPY_AND_ASSIGN(TopSites);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_TOP_SITES_H_
