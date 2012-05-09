// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EXPIRE_HISTORY_BACKEND_H_
#define CHROME_BROWSER_HISTORY_EXPIRE_HISTORY_BACKEND_H_
#pragma once

#include <queue>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/history/history_types.h"

class BookmarkService;
class GURL;
class TestingProfile;

namespace history {

class ArchivedDatabase;
class HistoryDatabase;
struct HistoryDetails;
class TextDatabaseManager;
class ThumbnailDatabase;

// Delegate used to broadcast notifications to the main thread.
class BroadcastNotificationDelegate {
 public:
  // Schedules a broadcast of the given notification on the application main
  // thread. The details argument will have ownership taken by this function.
  virtual void BroadcastNotifications(int type,
                                      HistoryDetails* details_deleted) = 0;

 protected:
  virtual ~BroadcastNotificationDelegate() {}
};

// Encapsulates visit expiration criteria and type of visits to expire.
class ExpiringVisitsReader {
 public:
  virtual ~ExpiringVisitsReader() {}
  // Populates |visits| from |db|, using provided |end_time| and |max_visits|
  // cap.
  virtual bool Read(base::Time end_time, HistoryDatabase* db,
                    VisitVector* visits, int max_visits) const = 0;
};

typedef std::vector<const ExpiringVisitsReader*> ExpiringVisitsReaders;

// Helper component to HistoryBackend that manages expiration and deleting of
// history, as well as moving data from the main database to the archived
// database as it gets old.
//
// It will automatically start periodically archiving old history once you call
// StartArchivingOldStuff().
class ExpireHistoryBackend {
 public:
  // The delegate pointer must be non-NULL. We will NOT take ownership of it.
  // BookmarkService may be NULL. The BookmarkService is used when expiring
  // URLs so that we don't remove any URLs or favicons that are bookmarked
  // (visits are removed though).
  ExpireHistoryBackend(BroadcastNotificationDelegate* delegate,
                       BookmarkService* bookmark_service);
  ~ExpireHistoryBackend();

  // Completes initialization by setting the databases that this class will use.
  void SetDatabases(HistoryDatabase* main_db,
                    ArchivedDatabase* archived_db,
                    ThumbnailDatabase* thumb_db,
                    TextDatabaseManager* text_db);

  // Begins periodic expiration of history older than the given threshold. This
  // will continue until the object is deleted.
  void StartArchivingOldStuff(base::TimeDelta expiration_threshold);

  // Deletes everything associated with a URL.
  void DeleteURL(const GURL& url);

  // Deletes everything associated with each URL in the list.
  void DeleteURLs(const std::vector<GURL>& url);

  // Removes all visits to restrict_urls (or all URLs if empty) in the given
  // time range, updating the URLs accordingly,
  void ExpireHistoryBetween(const std::set<GURL>& restrict_urls,
                            base::Time begin_time, base::Time end_time);

  // Removes the given list of visits, updating the URLs accordingly (similar to
  // ExpireHistoryBetween(), but affecting a specific set of visits).
  void ExpireVisits(const VisitVector& visits);

  // Archives all visits before and including the given time, updating the URLs
  // accordingly. This function is intended for migrating old databases
  // (which encompased all time) to the tiered structure and testing, and
  // probably isn't useful for anything else.
  void ArchiveHistoryBefore(base::Time end_time);

  // Returns the current time that we are archiving stuff to. This will return
  // the threshold in absolute time rather than a delta, so the caller should
  // not save it.
  base::Time GetCurrentArchiveTime() const {
    return base::Time::Now() - expiration_threshold_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ExpireHistoryTest, DeleteFaviconsIfPossible);
  FRIEND_TEST_ALL_PREFIXES(ExpireHistoryTest, ArchiveSomeOldHistory);
  FRIEND_TEST_ALL_PREFIXES(ExpireHistoryTest, ExpiringVisitsReader);
  FRIEND_TEST_ALL_PREFIXES(ExpireHistoryTest, ArchiveSomeOldHistoryWithSource);
  friend class ::TestingProfile;

  struct DeleteDependencies;

  // Deletes the visit-related stuff for all the visits in the given list, and
  // adds the rows for unique URLs affected to the affected_urls list in
  // the dependencies structure.
  //
  // Deleted information is the visits themselves and the full-text index
  // entries corresponding to them.
  void DeleteVisitRelatedInfo(const VisitVector& visits,
                              DeleteDependencies* dependencies);

  // Moves the given visits from the main database to the archived one.
  void ArchiveVisits(const VisitVector& visits);

  // Finds or deletes dependency information for the given URL. Information that
  // is specific to this URL (URL row, thumbnails, full text indexed stuff,
  // etc.) is deleted.
  //
  // This does not affect the visits! This is used for expiration as well as
  // deleting from the UI, and they handle visits differently.
  //
  // Other information will be collected and returned in the output containers.
  // This includes some of the things deleted that are needed elsewhere, plus
  // some things like favicons that could be shared by many URLs, and need to
  // be checked for deletion (this allows us to delete many URLs with only one
  // check for shared information at the end).
  //
  // Assumes the main_db_ is non-NULL.
  //
  // NOTE: If the url is bookmarked only the segments and text db are updated,
  // everything else is unchanged. This is done so that bookmarks retain their
  // favicons and thumbnails.
  void DeleteOneURL(const URLRow& url_row,
                    bool is_bookmarked,
                    DeleteDependencies* dependencies);

  // Adds or merges the given URL row with the archived database, returning the
  // ID of the URL in the archived database, or 0 on failure. The main (source)
  // database will not be affected (the URL will have to be deleted later).
  //
  // Assumes the archived database is not NULL.
  URLID ArchiveOneURL(const URLRow& url_row);

  // Deletes all the URLs in the given vector and handles their dependencies.
  // This will delete starred URLs
  void DeleteURLs(const URLRows& urls,
                  DeleteDependencies* dependencies);

  // Expiration involves removing visits, then propagating the visits out from
  // there and delete any orphaned URLs. These will be added to the deleted URLs
  // field of the dependencies and DeleteOneURL will handle deleting out from
  // there. This function does not handle favicons.
  //
  // When a URL is not deleted and |archive| is not set, the last visit time and
  // the visit and typed counts will be updated (we want to clear these when a
  // user is deleting history manually, but not when we're normally expiring old
  // things from history).
  //
  // The visits in the given vector should have already been deleted from the
  // database, and the list of affected URLs already be filled into
  // |depenencies->affected_urls|.
  //
  // Starred URLs will not be deleted. The information in the dependencies that
  // DeleteOneURL fills in will be updated, and this function will also delete
  // any now-unused favicons.
  void ExpireURLsForVisits(const VisitVector& visits,
                           DeleteDependencies* dependencies);

  // Creates entries in the archived database for the unique URLs referenced
  // by the given visits. It will then add versions of the visits to that
  // database. The source database WILL NOT BE MODIFIED. The source URLs and
  // visits will have to be deleted in another pass.
  //
  // The affected URLs will be filled into the given dependencies structure.
  void ArchiveURLsAndVisits(const VisitVector& visits,
                            DeleteDependencies* dependencies);

  // Deletes the favicons listed in the set if unused. Fails silently (we don't
  // care about favicons so much, so don't want to stop everything if it fails).
  void DeleteFaviconsIfPossible(const std::set<FaviconID>& favicon_id);

  // Enum representing what type of action resulted in the history DB deletion.
  enum DeletionType {
    // User initiated the deletion from the History UI.
    DELETION_USER_INITIATED,
    // History data was automatically archived due to being more than 90 days
    // old.
    DELETION_ARCHIVED
  };

  // Broadcast the URL deleted notification.
  void BroadcastDeleteNotifications(DeleteDependencies* dependencies,
                                    DeletionType type);

  // Schedules a call to DoArchiveIteration.
  void ScheduleArchive();

  // Calls ArchiveSomeOldHistory to expire some amount of old history, according
  // to the items in work queue, and schedules another call to happen in the
  // future.
  void DoArchiveIteration();

  // Tries to expire the oldest |max_visits| visits from history that are older
  // than |time_threshold|. The return value indicates if we think there might
  // be more history to expire with the current time threshold (it does not
  // indicate success or failure).
  bool ArchiveSomeOldHistory(base::Time end_time,
                             const ExpiringVisitsReader* reader,
                             int max_visits);

  // Tries to detect possible bad history or inconsistencies in the database
  // and deletes items. For example, URLs with no visits.
  void ParanoidExpireHistory();

  // Schedules a call to DoExpireHistoryIndexFiles.
  void ScheduleExpireHistoryIndexFiles();

  // Deletes old history index files.
  void DoExpireHistoryIndexFiles();

  // Returns the BookmarkService, blocking until it is loaded. This may return
  // NULL.
  BookmarkService* GetBookmarkService();

  // Initializes periodic expiration work queue by populating it with with tasks
  // for all known readers.
  void InitWorkQueue();

  // Returns the reader for all visits. This method is only used by the unit
  // tests.
  const ExpiringVisitsReader* GetAllVisitsReader();

  // Returns the reader for AUTO_SUBFRAME visits. This method is only used by
  // the unit tests.
  const ExpiringVisitsReader* GetAutoSubframeVisitsReader();

  // Non-owning pointer to the notification delegate (guaranteed non-NULL).
  BroadcastNotificationDelegate* delegate_;

  // Non-owning pointers to the databases we deal with (MAY BE NULL).
  HistoryDatabase* main_db_;       // Main history database.
  ArchivedDatabase* archived_db_;  // Old history.
  ThumbnailDatabase* thumb_db_;    // Thumbnails and favicons.
  TextDatabaseManager* text_db_;   // Full text index.

  // Used to generate runnable methods to do timers on this class. They will be
  // automatically canceled when this class is deleted.
  base::WeakPtrFactory<ExpireHistoryBackend> weak_factory_;

  // The threshold for "old" history where we will automatically expire it to
  // the archived database.
  base::TimeDelta expiration_threshold_;

  // List of all distinct types of readers. This list is used to populate the
  // work queue.
  ExpiringVisitsReaders readers_;

  // Work queue for periodic expiration tasks, used by DoArchiveIteration() to
  // determine what to do at an iteration, as well as populate it for future
  // iterations.
  std::queue<const ExpiringVisitsReader*> work_queue_;

  // Readers for various types of visits.
  // TODO(dglazkov): If you are adding another one, please consider reorganizing
  // into a map.
  scoped_ptr<ExpiringVisitsReader> all_visits_reader_;
  scoped_ptr<ExpiringVisitsReader> auto_subframe_visits_reader_;

  // The BookmarkService; may be null. This is owned by the Profile.
  //
  // Use GetBookmarkService to access this, which makes sure the service is
  // loaded.
  BookmarkService* bookmark_service_;

  DISALLOW_COPY_AND_ASSIGN(ExpireHistoryBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_EXPIRE_HISTORY_BACKEND_H_
