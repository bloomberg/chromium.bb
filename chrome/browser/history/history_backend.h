// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_BACKEND_H_
#define CHROME_BROWSER_HISTORY_HISTORY_BACKEND_H_

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/mru_cache.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/history/expire_history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_marshaling.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "chrome/browser/history/visit_tracker.h"
#include "components/search_engines/template_url_id.h"
#include "sql/init_status.h"

class TestingProfile;
class TypedUrlSyncableService;
struct ThumbnailScore;

namespace history {
#if defined(OS_ANDROID)
class AndroidProviderBackend;
#endif

class CommitLaterTask;
class HistoryClient;
class VisitFilter;
struct DownloadRow;

// The maximum number of icons URLs per page which can be stored in the
// thumbnail database.
static const size_t kMaxFaviconsPerPage = 8;

// The maximum number of bitmaps for a single icon URL which can be stored in
// the thumbnail database.
static const size_t kMaxFaviconBitmapsPerIconURL = 8;

// *See the .cc file for more information on the design.*
//
// Internal history implementation which does most of the work of the history
// system. This runs on a background thread (to not block the browser when we
// do expensive operations) and is NOT threadsafe, so it must only be called
// from message handlers on the background thread. Invoking on another thread
// requires threadsafe refcounting.
//
// Most functions here are just the implementations of the corresponding
// functions in the history service. These functions are not documented
// here, see the history service for behavior.
class HistoryBackend : public base::RefCountedThreadSafe<HistoryBackend>,
                       public BroadcastNotificationDelegate {
 public:
  // Interface implemented by the owner of the HistoryBackend object. Normally,
  // the history service implements this to send stuff back to the main thread.
  // The unit tests can provide a different implementation if they don't have
  // a history service object.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when the database cannot be read correctly for some reason.
    virtual void NotifyProfileError(sql::InitStatus init_status) = 0;

    // Sets the in-memory history backend. The in-memory backend is created by
    // the main backend. For non-unit tests, this happens on the background
    // thread. It is to be used on the main thread, so this would transfer
    // it to the history service. Unit tests can override this behavior.
    //
    // This function is NOT guaranteed to be called. If there is an error,
    // there may be no in-memory database.
    virtual void SetInMemoryBackend(
        scoped_ptr<InMemoryHistoryBackend> backend) = 0;

    // Broadcasts the specified notification to the notification service.
    // This is implemented here because notifications must only be sent from
    // the main thread. This is the only method that doesn't identify the
    // caller because notifications must always be sent.
    virtual void BroadcastNotifications(int type,
                                        scoped_ptr<HistoryDetails> details) = 0;

    // Invoked when the backend has finished loading the db.
    virtual void DBLoaded() = 0;

    virtual void NotifyVisitDBObserversOnAddVisit(
        const history::BriefVisitInfo& info) = 0;
  };

  // QueryURLResult stores the result of a call to QueryURL. The |row| and
  // |visits| fields are only valid if |success| is true.
  struct QueryURLResult {
    QueryURLResult();
    ~QueryURLResult();
    bool success;
    URLRow row;
    VisitVector visits;
  };

  // Init must be called to complete object creation. This object can be
  // constructed on any thread, but all other functions including Init() must
  // be called on the history thread.
  //
  // |history_dir| is the directory where the history files will be placed.
  // See the definition of BroadcastNotificationsCallback above. This function
  // takes ownership of the callback pointer.
  //
  // |history_client| is used to determine bookmarked URLs when deleting and
  // may be NULL.
  //
  // This constructor is fast and does no I/O, so can be called at any time.
  HistoryBackend(const base::FilePath& history_dir,
                 Delegate* delegate,
                 HistoryClient* history_client);

  // Must be called after creation but before any objects are created. If this
  // fails, all other functions will fail as well. (Since this runs on another
  // thread, we don't bother returning failure.)
  //
  // |languages| gives a list of language encodings with which the history
  // URLs and omnibox searches are interpreted.
  // |force_fail| can be set during unittests to unconditionally fail to init.
  void Init(const std::string& languages, bool force_fail);

  // Notification that the history system is shutting down. This will break
  // the refs owned by the delegate and any pending transaction so it will
  // actually be deleted.
  void Closing();

  void ClearCachedDataForContextID(ContextID context_id);

  // Navigation ----------------------------------------------------------------

  // |request.time| must be unique with high probability.
  void AddPage(const HistoryAddPageArgs& request);
  virtual void SetPageTitle(const GURL& url, const base::string16& title);
  void AddPageNoVisitForBookmark(const GURL& url, const base::string16& title);
  void UpdateWithPageEndTime(ContextID context_id,
                             int32 page_id,
                             const GURL& url,
                             base::Time end_ts);

  // Querying ------------------------------------------------------------------

  // ScheduleAutocomplete() never frees |provider| (which is globally live).
  // It passes |params| on to the autocomplete system which will eventually
  // free it.
  void ScheduleAutocomplete(HistoryURLProvider* provider,
                            HistoryURLProviderParams* params);

  void IterateURLs(
      const scoped_refptr<visitedlink::VisitedLinkDelegate::URLEnumerator>&
          enumerator);
  void QueryURL(const GURL& url,
                bool want_visits,
                QueryURLResult* query_url_result);
  void QueryHistory(scoped_refptr<QueryHistoryRequest> request,
                    const base::string16& text_query,
                    const QueryOptions& options);
  void QueryRedirectsFrom(scoped_refptr<QueryRedirectsRequest> request,
                          const GURL& url);
  void QueryRedirectsTo(scoped_refptr<QueryRedirectsRequest> request,
                        const GURL& url);

  void GetVisibleVisitCountToHost(
      scoped_refptr<GetVisibleVisitCountToHostRequest> request,
      const GURL& url);

  // TODO(Nik): remove. Use QueryMostVisitedURLs instead.
  void QueryTopURLsAndRedirects(
      scoped_refptr<QueryTopURLsAndRedirectsRequest> request,
      int result_count);

  // Request the |result_count| most visited URLs and the chain of
  // redirects leading to each of these URLs. |days_back| is the
  // number of days of history to use. Used by TopSites.
  void QueryMostVisitedURLs(
      scoped_refptr<QueryMostVisitedURLsRequest> request,
      int result_count,
      int days_back);

  // Request the |result_count| URLs and the chain of redirects
  // leading to each of these URLs, filterd and sorted based on the |filter|.
  // If |debug| is enabled, additional data will be computed and provided.
  void QueryFilteredURLs(
      scoped_refptr<QueryFilteredURLsRequest> request,
      int result_count,
      const history::VisitFilter& filter,
      bool debug);

  // QueryMostVisitedURLs without the request.
  void QueryMostVisitedURLsImpl(int result_count,
                                int days_back,
                                MostVisitedURLList* result);

  // Computes the most recent URL(s) that the given canonical URL has
  // redirected to and returns true on success. There may be more than one
  // redirect in a row, so this function will fill the given array with the
  // entire chain. If there are no redirects for the most recent visit of the
  // URL, or the URL is not in history, returns false.
  //
  // Backend for QueryRedirectsFrom.
  bool GetMostRecentRedirectsFrom(const GURL& url,
                                  history::RedirectList* redirects);

  // Similar to above function except computes a chain of redirects to the
  // given URL. Stores the most recent list of redirects ending at |url| in the
  // given RedirectList. For example, if we have the redirect list A -> B -> C,
  // then calling this function with url=C would fill redirects with {B, A}.
  bool GetMostRecentRedirectsTo(const GURL& url,
                                history::RedirectList* redirects);

  // Favicon -------------------------------------------------------------------

  void GetFavicons(
      const std::vector<GURL>& icon_urls,
      int icon_types,
      const std::vector<int>& desired_sizes,
      std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results);

  void GetLargestFaviconForURL(
      const GURL& page_url,
      const std::vector<int>& icon_types,
      int minimum_size_in_pixels,
      favicon_base::FaviconRawBitmapResult* bitmap_result);

  void GetFaviconsForURL(
      const GURL& page_url,
      int icon_types,
      const std::vector<int>& desired_sizes,
      std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results);

  void GetFaviconForID(
      favicon_base::FaviconID favicon_id,
      int desired_size,
      std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results);

  void UpdateFaviconMappingsAndFetch(
      const GURL& page_url,
      const std::vector<GURL>& icon_urls,
      int icon_types,
      const std::vector<int>& desired_sizes,
      std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results);

  void MergeFavicon(const GURL& page_url,
                    const GURL& icon_url,
                    favicon_base::IconType icon_type,
                    scoped_refptr<base::RefCountedMemory> bitmap_data,
                    const gfx::Size& pixel_size);

  void SetFavicons(const GURL& page_url,
                   favicon_base::IconType icon_type,
                   const std::vector<favicon_base::FaviconRawBitmapData>&
                       favicon_bitmap_data);

  void SetFaviconsOutOfDateForPage(const GURL& page_url);

  void CloneFavicons(const GURL& old_page_url, const GURL& new_page_url);

  void SetImportedFavicons(
      const std::vector<ImportedFaviconUsage>& favicon_usage);

  // Downloads -----------------------------------------------------------------

  uint32 GetNextDownloadId();
  void QueryDownloads(std::vector<DownloadRow>* rows);
  void UpdateDownload(const DownloadRow& data);
  bool CreateDownload(const history::DownloadRow& history_info);
  void RemoveDownloads(const std::set<uint32>& ids);

  // Segment usage -------------------------------------------------------------

  void QuerySegmentUsage(scoped_refptr<QuerySegmentUsageRequest> request,
                         const base::Time from_time,
                         int max_result_count);
  void DeleteOldSegmentData();

  // Keyword search terms ------------------------------------------------------

  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID keyword_id,
                                   const base::string16& term);

  void DeleteAllSearchTermsForKeyword(TemplateURLID keyword_id);

  void GetMostRecentKeywordSearchTerms(
      scoped_refptr<GetMostRecentKeywordSearchTermsRequest> request,
      TemplateURLID keyword_id,
      const base::string16& prefix,
      int max_count);

  void DeleteKeywordSearchTermForURL(const GURL& url);

  void DeleteMatchingURLsForKeyword(TemplateURLID keyword_id,
                                    const base::string16& term);

#if defined(OS_ANDROID)
  // Android Provider ---------------------------------------------------------

  // History and bookmarks ----------------------------------------------------
  void InsertHistoryAndBookmark(scoped_refptr<InsertRequest> request,
                                const HistoryAndBookmarkRow& row);

  void QueryHistoryAndBookmarks(
      scoped_refptr<QueryRequest> request,
      const std::vector<HistoryAndBookmarkRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<base::string16>& selection_args,
      const std::string& sort_order);

  void UpdateHistoryAndBookmarks(
      scoped_refptr<UpdateRequest> request,
      const HistoryAndBookmarkRow& row,
      const std::string& selection,
      const std::vector<base::string16>& selection_args);

  void DeleteHistoryAndBookmarks(
      scoped_refptr<DeleteRequest> request,
      const std::string& selection,
      const std::vector<base::string16>& selection_args);

  void DeleteHistory(scoped_refptr<DeleteRequest> request,
                     const std::string& selection,
                     const std::vector<base::string16>& selection_args);

  // Statement ----------------------------------------------------------------
  // Move the statement's current position.
  void MoveStatement(scoped_refptr<MoveStatementRequest> request,
                     history::AndroidStatement* statement,
                     int current_pos,
                     int destination);

  // Close the given statement. The ownership is transfered.
  void CloseStatement(AndroidStatement* statement);

  // Search terms -------------------------------------------------------------
  void InsertSearchTerm(scoped_refptr<InsertRequest> request,
                        const SearchRow& row);

  void UpdateSearchTerms(scoped_refptr<UpdateRequest> request,
                         const SearchRow& row,
                         const std::string& selection,
                         const std::vector<base::string16> selection_args);

  void DeleteSearchTerms(scoped_refptr<DeleteRequest> request,
                         const std::string& selection,
                         const std::vector<base::string16> selection_args);

  void QuerySearchTerms(scoped_refptr<QueryRequest> request,
                        const std::vector<SearchRow::ColumnID>& projections,
                        const std::string& selection,
                        const std::vector<base::string16>& selection_args,
                        const std::string& sort_order);

#endif  // defined(OS_ANDROID)

  // Generic operations --------------------------------------------------------

  void ProcessDBTask(scoped_refptr<HistoryDBTaskRequest> request);

  virtual bool GetAllTypedURLs(URLRows* urls);

  virtual bool GetVisitsForURL(URLID id, VisitVector* visits);

  // Fetches up to |max_visits| most recent visits for the passed URL.
  virtual bool GetMostRecentVisitsForURL(URLID id,
                                         int max_visits,
                                         VisitVector* visits);

  virtual bool UpdateURL(URLID id, const history::URLRow& url);

  // While adding visits in batch, the source needs to be provided.
  virtual bool AddVisits(const GURL& url,
                         const std::vector<history::VisitInfo>& visits,
                         VisitSource visit_source);

  virtual bool RemoveVisits(const VisitVector& visits);

  // Returns the VisitSource associated with each one of the passed visits.
  // If there is no entry in the map for a given visit, that means the visit
  // was SOURCE_BROWSED. Returns false if there is no HistoryDatabase..
  bool GetVisitsSource(const VisitVector& visits, VisitSourceMap* sources);

  virtual bool GetURL(const GURL& url, history::URLRow* url_row);

  // Returns the syncable service for syncing typed urls. The returned service
  // is owned by |this| object.
  virtual TypedUrlSyncableService* GetTypedUrlSyncableService() const;

  // Deleting ------------------------------------------------------------------

  virtual void DeleteURLs(const std::vector<GURL>& urls);

  virtual void DeleteURL(const GURL& url);

  // Calls ExpireHistoryBackend::ExpireHistoryBetween and commits the change.
  void ExpireHistoryBetween(
      const std::set<GURL>& restrict_urls,
      base::Time begin_time,
      base::Time end_time);

  // Finds the URLs visited at |times| and expires all their visits within
  // [|begin_time|, |end_time|). All times in |times| should be in
  // [|begin_time|, |end_time|). This is used when expiration request is from
  // server side, i.e. web history deletes, where only visit times (possibly
  // incomplete) are transmitted to protect user's privacy.
  void ExpireHistoryForTimes(const std::set<base::Time>& times,
                             base::Time begin_time, base::Time end_time);

  // Calls ExpireHistoryBetween() once for each element in the vector.
  // The fields of |ExpireHistoryArgs| map directly to the arguments of
  // of ExpireHistoryBetween().
  void ExpireHistory(const std::vector<ExpireHistoryArgs>& expire_list);

  // Bookmarks -----------------------------------------------------------------

  // Notification that a URL is no longer bookmarked. If there are no visits
  // for the specified url, it is deleted.
  void URLsNoLongerBookmarked(const std::set<GURL>& urls);

  // Callbacks To Kill Database When It Gets Corrupted -------------------------

  // Called by the database to report errors.  Schedules one call to
  // KillHistoryDatabase() in case of corruption.
  void DatabaseErrorCallback(int error, sql::Statement* stmt);

  // Raze the history database. It will be recreated in a future run. Hopefully
  // things go better then. Continue running but without reading or storing any
  // state into the HistoryBackend databases. Close all of the databases managed
  // HistoryBackend as there are no provisions for accessing the other databases
  // managed by HistoryBackend when the history database cannot be accessed.
  void KillHistoryDatabase();

  // Testing -------------------------------------------------------------------

  // Sets the task to run and the message loop to run it on when this object
  // is destroyed. See HistoryService::SetOnBackendDestroyTask for a more
  // complete description.
  void SetOnBackendDestroyTask(base::MessageLoop* message_loop,
                               const base::Closure& task);

  // Adds the given rows to the database if it doesn't exist. A visit will be
  // added for each given URL at the last visit time in the URLRow if the
  // passed visit type != SOURCE_SYNCED (the sync code manages visits itself).
  // Each visit will have the visit_source type set.
  void AddPagesWithDetails(const URLRows& info, VisitSource visit_source);

#if defined(UNIT_TEST)
  HistoryDatabase* db() const { return db_.get(); }

  ExpireHistoryBackend* expire_backend() { return &expirer_; }
#endif

  // Returns true if the passed visit time is already expired (used by the sync
  // code to avoid syncing visits that would immediately be expired).
  virtual bool IsExpiredVisitTime(const base::Time& time);

  base::Time GetFirstRecordedTimeForTest() {
    return first_recorded_time_;
  }

 protected:
  virtual ~HistoryBackend();

 private:
  friend class base::RefCountedThreadSafe<HistoryBackend>;
  friend class CommitLaterTask;  // The commit task needs to call Commit().
  friend class HistoryBackendTest;
  friend class HistoryBackendDBTest;  // So the unit tests can poke our innards.
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteAll);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteAllThenAddData);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPagesWithDetails);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, ImportedFaviconsTest);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, URLsNoLongerBookmarked);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, StripUsernamePasswordTest);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteThumbnailsDatabaseTest);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPageVisitSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPageVisitNotLastVisit);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           AddPageVisitFiresNotificationWithCorrectDetails);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPageArgsSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddVisitsSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetMostRecentVisits);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, RemoveVisitsSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, RemoveVisitsTransitions);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MigrationVisitSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           SetFaviconMappingsForPageAndRedirects);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           SetFaviconMappingsForPageDuplicates);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, SetFaviconsDeleteBitmaps);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, SetFaviconsReplaceBitmapData);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           SetFaviconsSameFaviconURLForTwoPages);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           UpdateFaviconMappingsAndFetchNoChange);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MergeFaviconPageURLNotInDB);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MergeFaviconPageURLInDB);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MergeFaviconMaxFaviconsPerPage);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           MergeFaviconIconURLMappedToDifferentPageURL);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           MergeFaviconMaxFaviconBitmapsPerIconURL);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           UpdateFaviconMappingsAndFetchMultipleIconTypes);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetFaviconsFromDBEmpty);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           GetFaviconsFromDBNoFaviconBitmaps);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           GetFaviconsFromDBSelectClosestMatch);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetFaviconsFromDBSingleIconURL);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetFaviconsFromDBIconType);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetFaviconsFromDBExpired);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           UpdateFaviconMappingsAndFetchNoDB);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           CloneFaviconIsRestrictedToSameDomain);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, QueryFilteredURLs);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, UpdateVisitDuration);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, ExpireHistoryForTimes);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteFTSIndexDatabases);

  friend class ::TestingProfile;

  // Computes the name of the specified database on disk.
  base::FilePath GetArchivedFileName() const;
  base::FilePath GetThumbnailFileName() const;

  // Returns the name of the Favicons database. This is the new name
  // of the Thumbnails database.
  base::FilePath GetFaviconsFileName() const;

#if defined(OS_ANDROID)
  // Returns the name of android cache database.
  base::FilePath GetAndroidCacheFileName() const;

  // Populate a map from a |MostVisitedURLList|. The map assigns a rank to each
  // top URL and its redirects. This should only be done once at backend
  // initialization.
  // This can be removed for M31. (See issue 248761.)

  void PopulateMostVisitedURLMap();
  // Record counts of page visits by rank. If a url is not ranked, record the
  // page visit in a slot corresponding to |max_top_url_count|, which should
  // be one greater than the largest rank of any url in |top_urls|.
  // This can be removed for M31. (See issue 248761.)
  void RecordTopPageVisitStats(const GURL& url);
#endif

  class URLQuerier;
  friend class URLQuerier;

  // Does the work of Init.
  void InitImpl(const std::string& languages);

  // Called when the system is under memory pressure.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Closes all databases managed by HistoryBackend. Commits any pending
  // transactions.
  void CloseAllDatabases();

  // Adds a single visit to the database, updating the URL information such
  // as visit and typed count. The visit ID of the added visit and the URL ID
  // of the associated URL (whether added or not) is returned. Both values will
  // be 0 on failure.
  //
  // This does not schedule database commits, it is intended to be used as a
  // subroutine for AddPage only. It also assumes the database is valid.
  std::pair<URLID, VisitID> AddPageVisit(const GURL& url,
                                         base::Time time,
                                         VisitID referring_visit,
                                         content::PageTransition transition,
                                         VisitSource visit_source);

  // Returns a redirect chain in |redirects| for the VisitID
  // |cur_visit|. |cur_visit| is assumed to be valid. Assumes that
  // this HistoryBackend object has been Init()ed successfully.
  void GetRedirectsFromSpecificVisit(
      VisitID cur_visit, history::RedirectList* redirects);

  // Similar to the above function except returns a redirect list ending
  // at |cur_visit|.
  void GetRedirectsToSpecificVisit(
      VisitID cur_visit, history::RedirectList* redirects);

  // Update the visit_duration information in visits table.
  void UpdateVisitDuration(VisitID visit_id, const base::Time end_ts);

  // Querying ------------------------------------------------------------------

  // Backends for QueryHistory. *Basic() handles queries that are not
  // text search queries and can just be given directly to the history DB.
  // The *Text() version performs a brute force query of the history DB to
  // search for results which match the given text query.
  // Both functions assume QueryHistory already checked the DB for validity.
  void QueryHistoryBasic(const QueryOptions& options, QueryResults* result);
  void QueryHistoryText(const base::string16& text_query,
                        const QueryOptions& options,
                        QueryResults* result);

  // Committing ----------------------------------------------------------------

  // We always keep a transaction open on the history database so that multiple
  // transactions can be batched. Periodically, these are flushed (use
  // ScheduleCommit). This function does the commit to write any new changes to
  // disk and opens a new transaction. This will be called automatically by
  // ScheduleCommit, or it can be called explicitly if a caller really wants
  // to write something to disk.
  void Commit();

  // Schedules a commit to happen in the future. We do this so that many
  // operations over a period of time will be batched together. If there is
  // already a commit scheduled for the future, this will do nothing.
  void ScheduleCommit();

  // Cancels the scheduled commit, if any. If there is no scheduled commit,
  // does nothing.
  void CancelScheduledCommit();

  // Segments ------------------------------------------------------------------

  // Walks back a segment chain to find the last visit with a non null segment
  // id and returns it. If there is none found, returns 0.
  SegmentID GetLastSegmentID(VisitID from_visit);

  // Update the segment information. This is called internally when a page is
  // added. Return the segment id of the segment that has been updated.
  SegmentID UpdateSegments(const GURL& url,
                           VisitID from_visit,
                           VisitID visit_id,
                           content::PageTransition transition_type,
                           const base::Time ts);

  // Favicons ------------------------------------------------------------------

  // Used by both UpdateFaviconMappingsAndFetch and GetFavicons.
  // If |page_url| is non-null, the icon urls for |page_url| (and all
  // redirects) are set to the subset of |icon_urls| for which icons are
  // already stored in the database.
  // If |page_url| is non-null, |icon_types| can be multiple icon types
  // only if |icon_types| == TOUCH_ICON | TOUCH_PRECOMPOSED_ICON.
  // If multiple icon types are specified, |page_url| will be mapped to the
  // icon URLs of the largest type available in the database.
  void UpdateFaviconMappingsAndFetchImpl(
      const GURL* page_url,
      const std::vector<GURL>& icon_urls,
      int icon_types,
      const std::vector<int>& desired_sizes,
      std::vector<favicon_base::FaviconRawBitmapResult>* results);

  // Set the favicon bitmaps for |icon_id|.
  // For each entry in |favicon_bitmap_data|, if a favicon bitmap already
  // exists at the entry's pixel size, replace the favicon bitmap's data with
  // the entry's bitmap data. Otherwise add a new favicon bitmap.
  // Any favicon bitmaps already mapped to |icon_id| whose pixel sizes are not
  // in |favicon_bitmap_data| are deleted.
  // If not NULL, |favicon_bitmaps_changed| is set to whether any of the bitmap
  // data at |icon_id| is changed as a result of calling this method.
  // Computing |favicon_bitmaps_changed| requires additional database queries
  // so should be avoided if unnecessary.
  void SetFaviconBitmaps(favicon_base::FaviconID icon_id,
                         const std::vector<favicon_base::FaviconRawBitmapData>&
                             favicon_bitmap_data,
                         bool* favicon_bitmaps_changed);

  // Returns true if |favicon_bitmap_data| passed to SetFavicons() is valid.
  // Criteria:
  // 1) |favicon_bitmap_data| contains no more than
  //      kMaxFaviconsPerPage unique icon URLs.
  //      kMaxFaviconBitmapsPerIconURL favicon bitmaps for each icon URL.
  // 2) FaviconRawBitmapData::bitmap_data contains non NULL bitmap data.
  bool ValidateSetFaviconsParams(const std::vector<
      favicon_base::FaviconRawBitmapData>& favicon_bitmap_data) const;

  // Returns true if the bitmap data at |bitmap_id| equals |new_bitmap_data|.
  bool IsFaviconBitmapDataEqual(
      FaviconBitmapID bitmap_id,
      const scoped_refptr<base::RefCountedMemory>& new_bitmap_data);

  // Returns true if there are favicons for |page_url| and one of the types in
  // |icon_types|.
  // |favicon_bitmap_results| is set to the favicon bitmaps whose edge sizes
  // most closely match |desired_sizes|. If |desired_sizes| has a '0' entry, the
  // largest favicon bitmap with one of the icon types in |icon_types| is
  // returned. If |icon_types| contains multiple icon types and there are
  // several matched icon types in the database, results will only be returned
  // for a single icon type in the priority of TOUCH_PRECOMPOSED_ICON,
  // TOUCH_ICON, and FAVICON. See the comment for
  // GetFaviconResultsForBestMatch() for more details on how
  // |favicon_bitmap_results| is constructed.
  bool GetFaviconsFromDB(
      const GURL& page_url,
      int icon_types,
      const std::vector<int>& desired_sizes,
      std::vector<favicon_base::FaviconRawBitmapResult>*
          favicon_bitmap_results);

  // Returns the favicon bitmaps whose edge sizes most closely match
  // |desired_sizes| in |favicon_bitmap_results|. If |desired_sizes| has a '0'
  // entry, only the largest favicon bitmap is returned. Goodness is computed
  // via SelectFaviconFrameIndices(). It is computed on a per FaviconID basis,
  // thus all |favicon_bitmap_results| are guaranteed to be for the same
  // FaviconID. |favicon_bitmap_results| will have at most one entry for each
  // desired edge size. There will be fewer entries if the same favicon bitmap
  // is the best result for multiple edge sizes.
  // Returns true if there were no errors.
  bool GetFaviconBitmapResultsForBestMatch(
      const std::vector<favicon_base::FaviconID>& candidate_favicon_ids,
      const std::vector<int>& desired_sizes,
      std::vector<favicon_base::FaviconRawBitmapResult>*
          favicon_bitmap_results);

  // Maps the favicon ids in |icon_ids| to |page_url| (and all redirects)
  // for |icon_type|.
  // Returns true if the mappings for the page or any of its redirects were
  // changed.
  bool SetFaviconMappingsForPageAndRedirects(
      const GURL& page_url,
      favicon_base::IconType icon_type,
      const std::vector<favicon_base::FaviconID>& icon_ids);

  // Maps the favicon ids in |icon_ids| to |page_url| for |icon_type|.
  // Returns true if the function changed some of |page_url|'s mappings.
  bool SetFaviconMappingsForPage(
      const GURL& page_url,
      favicon_base::IconType icon_type,
      const std::vector<favicon_base::FaviconID>& icon_ids);

  // Returns all the page URLs in the redirect chain for |page_url|. If there
  // are no known redirects for |page_url|, returns a vector with |page_url|.
  void GetCachedRecentRedirects(const GURL& page_url,
                                history::RedirectList* redirect_list);

  // Send notification that the favicon has changed for |page_url| and all its
  // redirects.
  void SendFaviconChangedNotificationForPageAndRedirects(
      const GURL& page_url);

  // Generic stuff -------------------------------------------------------------

  // Processes the next scheduled HistoryDBTask, scheduling this method
  // to be invoked again if there are more tasks that need to run.
  void ProcessDBTaskImpl();

  // Release all tasks in history_db_tasks_ and clears it.
  void ReleaseDBTasks();

  virtual void BroadcastNotifications(
      int type,
      scoped_ptr<HistoryDetails> details) OVERRIDE;
  virtual void NotifySyncURLsModified(URLRows* rows) OVERRIDE;
  virtual void NotifySyncURLsDeleted(bool all_history,
                                     bool expired,
                                     URLRows* rows) OVERRIDE;

  // Deleting all history ------------------------------------------------------

  // Deletes all history. This is a special case of deleting that is separated
  // from our normal dependency-following method for performance reasons. The
  // logic lives here instead of ExpireHistoryBackend since it will cause
  // re-initialization of some databases (e.g. Thumbnails) that could fail.
  // When these databases are not valid, our pointers must be NULL, so we need
  // to handle this type of operation to keep the pointers in sync.
  void DeleteAllHistory();

  // Given a vector of all URLs that we will keep, removes all thumbnails
  // referenced by any URL, and also all favicons that aren't used by those
  // URLs.
  bool ClearAllThumbnailHistory(const URLRows& kept_urls);

  // Deletes all information in the history database, except for the supplied
  // set of URLs in the URL table (these should correspond to the bookmarked
  // URLs).
  //
  // The IDs of the URLs may change.
  bool ClearAllMainHistory(const URLRows& kept_urls);

  // Deletes the FTS index database files, which are no longer used.
  void DeleteFTSIndexDatabases();

  // Returns the HistoryClient, blocking until the bookmarks are loaded. This
  // may return NULL during testing.
  HistoryClient* GetHistoryClient();

  // Notify any observers of an addition to the visit database.
  void NotifyVisitObservers(const VisitRow& visit);

  // Data ----------------------------------------------------------------------

  // Delegate. See the class definition above for more information. This will
  // be NULL before Init is called and after Cleanup, but is guaranteed
  // non-NULL in between.
  scoped_ptr<Delegate> delegate_;

  // Directory where database files will be stored.
  base::FilePath history_dir_;

  // The history/thumbnail databases. Either MAY BE NULL if the database could
  // not be opened, all users must first check for NULL and return immediately
  // if it is. The thumbnail DB may be NULL when the history one isn't, but not
  // vice-versa.
  scoped_ptr<HistoryDatabase> db_;
  bool scheduled_kill_db_;  // Database is being killed due to error.
  scoped_ptr<ThumbnailDatabase> thumbnail_db_;

  // Manages expiration between the various databases.
  ExpireHistoryBackend expirer_;

  // A commit has been scheduled to occur sometime in the future. We can check
  // non-null-ness to see if there is a commit scheduled in the future, and we
  // can use the pointer to cancel the scheduled commit. There can be only one
  // scheduled commit at a time (see ScheduleCommit).
  scoped_refptr<CommitLaterTask> scheduled_commit_;

  // Maps recent redirect destination pages to the chain of redirects that
  // brought us to there. Pages that did not have redirects or were not the
  // final redirect in a chain will not be in this list, as well as pages that
  // redirected "too long" ago (as determined by ExpireOldRedirects above).
  // It is used to set titles & favicons for redirects to that of the
  // destination.
  //
  // As with AddPage, the last item in the redirect chain will be the
  // destination of the redirect (i.e., the key into recent_redirects_);
  typedef base::MRUCache<GURL, history::RedirectList> RedirectCache;
  RedirectCache recent_redirects_;

  // Timestamp of the first entry in our database.
  base::Time first_recorded_time_;

  // When set, this is the task that should be invoked on destruction.
  base::MessageLoop* backend_destroy_message_loop_;
  base::Closure backend_destroy_task_;

  // Tracks page transition types.
  VisitTracker tracker_;

  // A boolean variable to track whether we have already purged obsolete segment
  // data.
  bool segment_queried_;

  // HistoryDBTasks to run. Be sure to AddRef when adding, and Release when
  // done.
  std::list<HistoryDBTaskRequest*> db_task_requests_;

  // Used to determine if a URL is bookmarked; may be NULL.
  //
  // Use GetHistoryClient to access this, which makes sure the bookmarks are
  // loaded before returning.
  HistoryClient* history_client_;

#if defined(OS_ANDROID)
  // Used to provide the Android ContentProvider APIs.
  scoped_ptr<AndroidProviderBackend> android_provider_backend_;

  // Used to provide UMA on the number of page visits that are to the most
  // visited URLs. This is here because the backend both has access to this
  // information and is notified of page visits. The top sites service should
  // be used instead whenever possible.
  std::map<GURL, int> most_visited_urls_map_;
#endif

  // Used to manage syncing of the typed urls datatype. This will be NULL
  // before Init is called.
  scoped_ptr<TypedUrlSyncableService> typed_url_syncable_service_;

  // Listens for the system being under memory pressure.
  scoped_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  DISALLOW_COPY_AND_ASSIGN(HistoryBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_BACKEND_H_
