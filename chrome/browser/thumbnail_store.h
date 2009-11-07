// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THUMBNAIL_STORE_H_
#define CHROME_BROWSER_THUMBNAIL_STORE_H_

#include <map>
#include <string>
#include <vector>

#include "app/sql/connection.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/timer.h"
#include "chrome/browser/cancelable_request.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/url_database.h"  // For DBCloseScoper
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/ref_counted_util.h"
#include "chrome/common/thumbnail_score.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class DictionaryValue;
class GURL;
class HistoryService;
class Profile;
class SkBitmap;
struct sqlite3;
namespace base {
class Time;
}

// This storage interface provides storage for the thumbnails used
// by the new_tab_ui.
class ThumbnailStore : public base::RefCountedThreadSafe<ThumbnailStore>,
                       public NotificationObserver {
 public:
  ThumbnailStore();

  // Must be called before {Set,Get}PageThumbnail.  |db_name| is the location
  // of an existing ThumbnailStore database or where to create a new one.
  void Init(const FilePath& db_name, Profile* profile);

  // Is the ThumbnailStore ready for GetPageThumbnail requests?
  bool IsReady() { return disk_data_loaded_ && redirect_urls_.get(); }

  // Stores the given thumbnail and score with the associated url in the cache.
  bool SetPageThumbnail(const GURL& url,
                        const SkBitmap& thumbnail,
                        const ThumbnailScore& score,
                        bool fetch_redirects);  // for debugging

  // Sets *data to point to the thumbnail for the given url.
  // Returns false if no thumbnail available.
  bool GetPageThumbnail(const GURL& url, RefCountedBytes** data);

  // This is called when the browser is shutting down to write all dirty cache
  // entries to disk.
  void Shutdown();

 private:
  friend class base::RefCountedThreadSafe<ThumbnailStore>;
  FRIEND_TEST(ThumbnailStoreTest, RetrieveFromCache);
  FRIEND_TEST(ThumbnailStoreTest, RetrieveFromDisk);
  FRIEND_TEST(ThumbnailStoreTest, UpdateThumbnail);
  FRIEND_TEST(ThumbnailStoreTest, FollowRedirects);
  friend class ThumbnailStoreTest;

  ~ThumbnailStore();

  struct CacheEntry {
    scoped_refptr<RefCountedBytes> data_;
    ThumbnailScore score_;
    bool dirty_;

    CacheEntry() : data_(NULL), score_(ThumbnailScore()), dirty_(false) {}
    CacheEntry(RefCountedBytes* data,
               const ThumbnailScore& score,
               bool dirty)
        : data_(data),
          score_(score),
          dirty_(dirty) {}
  };

  // Data structure used to store thumbnail data in memory.
  typedef std::map<GURL, CacheEntry> Cache;

  // Data structre used to store the top visited URLs. It maps the end of a
  // redirect chain to the beginning. If A -> B -> C is a redirect chain and C
  // is a top visited url, then this map will contain an entry C => A.
  typedef std::map<GURL, GURL> MostVisitedMap;

  // NotificationObserver implementation
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Notify anyone listening that the ThumbnailStore is ready to be used.
  void NotifyThumbnailStoreReady();

  // Most visited URLs and their redirect lists -------------------------------

  // Query the HistoryService for the most visited URLs and the most recent
  // redirect lists for those URLs.  This happens in the background and the
  // callback is OnURLDataAvailable.
  void UpdateURLData();

  // The callback for UpdateURLData.
  void OnURLDataAvailable(HistoryService::Handle handle,
                          bool success,
                          std::vector<GURL>* urls,
                          history::RedirectMap* redirects);

  // The callback for the redirects request to the HistoryService made in
  // SetPageThumbnail. If we have a redirect chain A -> B -> C, this function
  // will be called with url=C and redirects = {B -> A}.  This information gets
  // inserted into the RedirectMap as A => {B -> C}.
  void OnRedirectsForURLAvailable(HistoryService::Handle handle,
                                  GURL url,
                                  bool success,
                                  history::RedirectList* redirects);

  // Search the RedirectMap for a redirect chain ending at |url|.  Returns an
  // iterator to an entry in redirect_urls_ if found or redirect_urls_->end().
  history::RedirectMap::iterator GetRedirectIteratorForURL(
      const GURL& url) const;

  // Remove stale data --------------------------------------------------------

  // Remove entries from the in memory thumbnail cache cache that have been
  // blacklisted or are not in the top kMaxCacheSize visited sites.  Call
  // CommitCacheToDB on the file_thread to remove these entries from disk and
  // to also write new entries to disk.
  void CleanCacheData();

  // Disk operations ----------------------------------------------------------

  // Initialize |db_| to the database specified in |db_name|.  If |cb_loop|
  // is non-null, calls GetAllThumbnailsFromDisk.  Done on the file_thread.
  void InitializeFromDB(const FilePath& db_name, MessageLoop* cb_loop);

  // Read all thumbnail data from the specified FilePath into a Cache object.
  // Done on the file_thread and returns to OnDiskDataAvailable on the thread
  // owning the specified MessageLoop.
  void GetAllThumbnailsFromDisk(MessageLoop* cb_loop);

  // Once thumbnail data from the disk is available from the file_thread,
  // this function is invoked on the main thread.  It takes ownership of the
  // Cache* passed in and retains this Cache* for the lifetime of the object.
  void OnDiskDataAvailable(Cache* cache);

  // Delete each URL in the given vector from the DB and write all dirty
  // cache entries to the DB.
  void CommitCacheToDB(
      scoped_refptr<RefCountedVector<GURL> > urls_to_delete,
      Cache* data);

  // Decide whether to store data ---------------------------------------------

  bool ShouldStoreThumbnailForURL(const GURL& url) const;

  bool IsURLBlacklisted(const GURL& url) const;

  std::wstring GetDictionaryKeyForURL(const std::string& url) const;

  // Returns true if url is in most_visited_urls_.
  bool IsPopular(const GURL& url) const;



  // Member variables ---------------------------------------------------------

  // The Cache maintained by the object.
  scoped_ptr<Cache> cache_;

  // The database holding the thumbnails on disk.
  sql::Connection db_;

  // We hold a reference to the history service to query for most visited URLs
  // and redirect information.
  scoped_refptr<HistoryService> hs_;

  // The most visited urls refreshed every kUpdateIntervalSecs from the
  // HistoryService.
  scoped_ptr<MostVisitedMap> most_visited_urls_;

  // A pointer to the persistent URL blacklist for this profile.
  const DictionaryValue* url_blacklist_;

  // A map pairing the URL that a user typed to a list of URLs it was
  // redirected to. Ex:
  // google.com => { http://www.google.com/ }
  scoped_ptr<history::RedirectMap> redirect_urls_;

  // Timer on which UpdateURLData runs.
  base::RepeatingTimer<ThumbnailStore> timer_;

  // Consumer for queries to the HistoryService.
  CancelableRequestConsumer consumer_;

  // Registrar to get notified when the history is cleared.
  NotificationRegistrar registrar_;

  static const unsigned int kMaxCacheSize = 24;
  static const int64 kUpdateIntervalSecs = 360;

  // Has the data from disk been read?
  bool disk_data_loaded_;

  DISALLOW_COPY_AND_ASSIGN(ThumbnailStore);
};

#endif  // CHROME_BROWSER_THUMBNAIL_STORE_H_
