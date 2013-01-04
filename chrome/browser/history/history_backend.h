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
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/history/archived_database.h"
#include "chrome/browser/history/expire_history_backend.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/history_marshaling.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/text_database_manager.h"
#include "chrome/browser/history/thumbnail_database.h"
#include "chrome/browser/history/visit_tracker.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "sql/init_status.h"
#include "ui/base/layout.h"

class BookmarkService;
class TestingProfile;
struct ThumbnailScore;

namespace history {
#if defined(OS_ANDROID)
class AndroidProviderBackend;
#endif

class CommitLaterTask;
class HistoryPublisher;
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
    virtual void NotifyProfileError(int backend_id,
                                    sql::InitStatus init_status) = 0;

    // Sets the in-memory history backend. The in-memory backend is created by
    // the main backend. For non-unit tests, this happens on the background
    // thread. It is to be used on the main thread, so this would transfer
    // it to the history service. Unit tests can override this behavior.
    //
    // This function is NOT guaranteed to be called. If there is an error,
    // there may be no in-memory database.
    //
    // Ownership of the backend pointer is transferred to this function.
    virtual void SetInMemoryBackend(int backend_id,
                                    InMemoryHistoryBackend* backend) = 0;

    // Broadcasts the specified notification to the notification service.
    // This is implemented here because notifications must only be sent from
    // the main thread. This is the only method that doesn't identify the
    // caller because notifications must always be sent.
    //
    // Ownership of the HistoryDetails is transferred to this function.
    virtual void BroadcastNotifications(int type,
                                        HistoryDetails* details) = 0;

    // Invoked when the backend has finished loading the db.
    virtual void DBLoaded(int backend_id) = 0;

    // Tell TopSites to start reading thumbnails from the ThumbnailsDB.
    virtual void StartTopSitesMigration(int backend_id) = 0;

    virtual void NotifyVisitDBObserversOnAddVisit(
        const history::BriefVisitInfo& info) = 0;
  };

  // Init must be called to complete object creation. This object can be
  // constructed on any thread, but all other functions including Init() must
  // be called on the history thread.
  //
  // |history_dir| is the directory where the history files will be placed.
  // See the definition of BroadcastNotificationsCallback above. This function
  // takes ownership of the callback pointer.
  //
  // |id| is used to communicate with the delegate, to identify which
  // backend is calling the method.
  //
  // |bookmark_service| is used to determine bookmarked URLs when deleting and
  // may be NULL.
  //
  // This constructor is fast and does no I/O, so can be called at any time.
  HistoryBackend(const FilePath& history_dir,
                 int id,
                 Delegate* delegate,
                 BookmarkService* bookmark_service);

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

  // See NotifyRenderProcessHostDestruction.
  void NotifyRenderProcessHostDestruction(const void* host);

  // Navigation ----------------------------------------------------------------

  // |request.time| must be unique with high probability.
  void AddPage(const HistoryAddPageArgs& request);
  virtual void SetPageTitle(const GURL& url, const string16& title);
  void AddPageNoVisitForBookmark(const GURL& url, const string16& title);

  // Updates the database backend with a page's ending time stamp information.
  // The page can be identified by the combination of the pointer to
  // a RenderProcessHost, the page id and the url.
  //
  // The given pointer will not be dereferenced, it is only used for
  // identification purposes, hence it is a void*.
  void UpdateWithPageEndTime(const void* host,
                             int32 page_id,
                             const GURL& url,
                             base::Time end_ts);


  // Indexing ------------------------------------------------------------------

  void SetPageContents(const GURL& url, const string16& contents);

  // Querying ------------------------------------------------------------------

  // ScheduleAutocomplete() never frees |provider| (which is globally live).
  // It passes |params| on to the autocomplete system which will eventually
  // free it.
  void ScheduleAutocomplete(HistoryURLProvider* provider,
                            HistoryURLProviderParams* params);

  void IterateURLs(VisitedLinkDelegate::URLEnumerator* enumerator);
  void QueryURL(scoped_refptr<QueryURLRequest> request,
                const GURL& url,
                bool want_visits);
  void QueryHistory(scoped_refptr<QueryHistoryRequest> request,
                    const string16& text_query,
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

  // Thumbnails ----------------------------------------------------------------

  void SetPageThumbnail(const GURL& url,
                        const gfx::Image* thumbnail,
                        const ThumbnailScore& score);

  // Retrieves a thumbnail, passing it across thread boundaries
  // via. the included callback.
  void GetPageThumbnail(scoped_refptr<GetPageThumbnailRequest> request,
                        const GURL& page_url);

  // Backend implementation of GetPageThumbnail. Unlike
  // GetPageThumbnail(), this method has way to transport data across
  // thread boundaries.
  //
  // Exposed for testing reasons.
  void GetPageThumbnailDirectly(
      const GURL& page_url,
      scoped_refptr<base::RefCountedBytes>* data);

  void MigrateThumbnailsDatabase();

  // Favicon -------------------------------------------------------------------

  struct FaviconResults {
    FaviconResults();
    ~FaviconResults();
    void Clear();

    std::vector<history::FaviconBitmapResult> bitmap_results;
    IconURLSizesMap size_map;
  };

  void GetFavicons(const std::vector<GURL>& icon_urls,
                    int icon_types,
                    int desired_size_in_dip,
                    const std::vector<ui::ScaleFactor>& desired_scale_factors,
                    FaviconResults* results);

  void GetFaviconsForURL(
      const GURL& page_url,
      int icon_types,
      int desired_size_in_dip,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      FaviconResults* results);

  void GetFaviconForID(
      FaviconID favicon_id,
      int desired_size_in_dip,
      ui::ScaleFactor desired_scale_factor,
      FaviconResults* results);

  void UpdateFaviconMappingsAndFetch(
      const GURL& page_url,
      const std::vector<GURL>& icon_urls,
      int icon_types,
      int desired_size_in_dip,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      FaviconResults* results);

  void MergeFavicon(const GURL& page_url,
                    const GURL& icon_url,
                    IconType icon_type,
                    scoped_refptr<base::RefCountedMemory> bitmap_data,
                    const gfx::Size& pixel_size);

  void SetFavicons(
      const GURL& page_url,
      IconType icon_type,
      const std::vector<FaviconBitmapData>& favicon_bitmap_data,
      const IconURLSizesMap& icon_url_sizes);

  void SetFaviconsOutOfDateForPage(const GURL& page_url);

  void CloneFavicons(const GURL& old_page_url, const GURL& new_page_url);

  void SetImportedFavicons(
      const std::vector<ImportedFaviconUsage>& favicon_usage);

  // Downloads -----------------------------------------------------------------

  void GetNextDownloadId(int* id);
  void QueryDownloads(std::vector<DownloadRow>* rows);
  void CleanUpInProgressEntries();
  void UpdateDownload(const DownloadRow& data);
  void CreateDownload(const history::DownloadRow& history_info,
                      int64* db_handle);
  void RemoveDownloads(const std::set<int64>& db_handles);

  // Segment usage -------------------------------------------------------------

  void QuerySegmentUsage(scoped_refptr<QuerySegmentUsageRequest> request,
                         const base::Time from_time,
                         int max_result_count);
  void DeleteOldSegmentData();
  void SetSegmentPresentationIndex(SegmentID segment_id, int index);

  // Keyword search terms ------------------------------------------------------

  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID keyword_id,
                                   const string16& term);

  void DeleteAllSearchTermsForKeyword(TemplateURLID keyword_id);

  void GetMostRecentKeywordSearchTerms(
      scoped_refptr<GetMostRecentKeywordSearchTermsRequest> request,
      TemplateURLID keyword_id,
      const string16& prefix,
      int max_count);

#if defined(OS_ANDROID)
  // Android Provider ---------------------------------------------------------

  // History and bookmarks ----------------------------------------------------
  void InsertHistoryAndBookmark(scoped_refptr<InsertRequest> request,
                                const HistoryAndBookmarkRow& row);

  void QueryHistoryAndBookmarks(
      scoped_refptr<QueryRequest> request,
      const std::vector<HistoryAndBookmarkRow::ColumnID>& projections,
      const std::string& selection,
      const std::vector<string16>& selection_args,
      const std::string& sort_order);

  void UpdateHistoryAndBookmarks(scoped_refptr<UpdateRequest> request,
                                 const HistoryAndBookmarkRow& row,
                                 const std::string& selection,
                                 const std::vector<string16>& selection_args);

  void DeleteHistoryAndBookmarks(scoped_refptr<DeleteRequest> request,
                                 const std::string& selection,
                                 const std::vector<string16>& selection_args);

  void DeleteHistory(scoped_refptr<DeleteRequest> request,
                     const std::string& selection,
                     const std::vector<string16>& selection_args);

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
                         const std::vector<string16> selection_args);

  void DeleteSearchTerms(scoped_refptr<DeleteRequest> request,
                         const std::string& selection,
                         const std::vector<string16> selection_args);

  void QuerySearchTerms(scoped_refptr<QueryRequest> request,
                        const std::vector<SearchRow::ColumnID>& projections,
                        const std::string& selection,
                        const std::vector<string16>& selection_args,
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

  // Deleting ------------------------------------------------------------------

  virtual void DeleteURLs(const std::vector<GURL>& urls);

  virtual void DeleteURL(const GURL& url);

  // Calls ExpireHistoryBackend::ExpireHistoryBetween and commits the change.
  void ExpireHistoryBetween(
      const std::set<GURL>& restrict_urls,
      base::Time begin_time,
      base::Time end_time);

  // Calls ExpireHistoryBackend::ExpireHistoryForTimes and commits the change.
  void ExpireHistoryForTimes(const std::vector<base::Time>& times);

  // Bookmarks -----------------------------------------------------------------

  // Notification that a URL is no longer bookmarked. If there are no visits
  // for the specified url, it is deleted.
  void URLsNoLongerBookmarked(const std::set<GURL>& urls);

  // Callbacks To Kill Database When It Gets Corrupted -------------------------

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
  void SetOnBackendDestroyTask(MessageLoop* message_loop,
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
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, ImportedFaviconsTest);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, URLsNoLongerBookmarked);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, StripUsernamePasswordTest);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, DeleteThumbnailsDatabaseTest);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPageVisitSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddPageArgsSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, AddVisitsSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, GetMostRecentVisits);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, RemoveVisitsSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, RemoveVisitsTransitions);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MigrationVisitSource);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, MigrationIconMapping);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           SetFaviconMappingsForPageAndRedirects);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest,
                           SetFaviconMappingsForPageDuplicates);
  FRIEND_TEST_ALL_PREFIXES(HistoryBackendTest, SetFavicons);
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

  friend class ::TestingProfile;

  // Computes the name of the specified database on disk.
  FilePath GetThumbnailFileName() const;

  // Returns the name of the Favicons database. This is the new name
  // of the Thumbnails database.
  // See ThumbnailDatabase::RenameAndDropThumbnails.
  FilePath GetFaviconsFileName() const;
  FilePath GetArchivedFileName() const;

#if defined(OS_ANDROID)
  // Returns the name of android cache database.
  FilePath GetAndroidCacheFileName() const;
#endif

  class URLQuerier;
  friend class URLQuerier;

  // Does the work of Init.
  void InitImpl(const std::string& languages);

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

  // Thumbnail Helpers ---------------------------------------------------------

  // When a simple GetMostRecentRedirectsFrom() fails, this method is
  // called which searches the last N visit sessions instead of just
  // the current one. Returns true and puts thumbnail data in |data|
  // if a proper thumbnail was found. Returns false otherwise. Assumes
  // that this HistoryBackend object has been Init()ed successfully.
  bool GetThumbnailFromOlderRedirect(
      const GURL& page_url, std::vector<unsigned char>* data);

  // Querying ------------------------------------------------------------------

  // Backends for QueryHistory. *Basic() handles queries that are not FTS (full
  // text search) queries and can just be given directly to the history DB).
  // The FTS version queries the text_database, then merges with the history DB.
  // Both functions assume QueryHistory already checked the DB for validity.
  void QueryHistoryBasic(URLDatabase* url_db, VisitDatabase* visit_db,
                         const QueryOptions& options, QueryResults* result);
  void QueryHistoryFTS(const string16& text_query,
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
      int desired_size_in_dip,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      FaviconResults* results);

  // Set the favicon bitmaps for |icon_id|.
  // For each entry in |favicon_bitmap_data|, if a favicon bitmap already
  // exists at the entry's pixel size, replace the favicon bitmap's data with
  // the entry's bitmap data. Otherwise add a new favicon bitmap.
  void SetFaviconBitmaps(
      FaviconID icon_id,
      const std::vector<FaviconBitmapData>& favicon_bitmap_data);

  // Returns true if |favicon_bitmap_data| and |icon_url_sizes| passed to
  // SetFavicons() are valid.
  // Criteria:
  // 1) |icon_url_sizes| contains no more than
  //      kMaxFaviconsPerPage icon URLs.
  //      kMaxFaviconBitmapsPerIconURL favicon sizes for each icon URL.
  // 2) The icon URLs and favicon sizes of |favicon_bitmap_data| are a subset
  //    of |icon_url_sizes|.
  // 3) The favicon sizes for entries in |icon_url_sizes| which have associated
  //    data in |favicon_bitmap_data| is not history::GetDefaultFaviconSizes().
  // 4) FaviconBitmapData::bitmap_data contains non NULL bitmap data.
  bool ValidateSetFaviconsParams(
      const std::vector<FaviconBitmapData>& favicon_bitmap_data,
      const IconURLSizesMap& icon_url_sizes) const;

  // Sets the sizes that the thumbnail database knows that the favicon at
  // |icon_id| is available from the web. See history_types.h for a more
  // detailed description of FaviconSizes.
  // Deletes any favicon bitmaps currently mapped to |icon_id| whose pixel
  // sizes are not contained in |favicon_sizes|.
  void SetFaviconSizes(FaviconID icon_id,
                       const FaviconSizes& favicon_sizes);

  // Returns true if there are favicons for |page_url| and one of the types in
  // |icon_types|.
  // |favicon_bitmap_results| is set to the favicon bitmaps which most closely
  // match |desired_size_in_dip| and |desired_scale_factors|. If
  // |desired_size_in_dip| is 0, the largest favicon bitmap with one of the icon
  // types in |icon_types| is returned. If |icon_types| contains multiple icon
  // types and there are several matched icon types in the database, results
  // will only be returned for a single icon type in the priority of
  // TOUCH_PRECOMPOSED_ICON, TOUCH_ICON, and FAVICON. See the comment for
  // GetFaviconResultsForBestMatch() for more details on how
  // |favicon_bitmap_results| is constructed.
  // |icon_url_sizes| is set to a mapping of all the icon URLs which are mapped
  // to |page_url| to the sizes of the favicon bitmaps available at each icon
  // URL on the web.
  bool GetFaviconsFromDB(
      const GURL& page_url,
      int icon_types,
      const int desired_size_in_dip,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      std::vector<FaviconBitmapResult>* favicon_bitmap_results,
      IconURLSizesMap* icon_url_sizes);

  // Returns the favicon bitmaps which most closely match |desired_size_in_dip|
  // and |desired_scale_factors| in |favicon_bitmap_results|. If
  // |desired_size_in_dip| is 0, only the largest favicon bitmap is returned.
  // Goodness is computed via SelectFaviconBitmapIDs(). It is computed on a
  // per favicon id basis, thus all |favicon_bitmap_results| are guaranteed to
  // be for the same FaviconID. |favicon_bitmap_results| will have at most one
  // entry for each desired scale factor. There will be less entries if the same
  // favicon bitmap is the best result for multiple scale factors.
  // Returns true if there were no errors.
  bool GetFaviconBitmapResultsForBestMatch(
      const std::vector<FaviconID>& candidate_favicon_ids,
      int desired_size_in_dip,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      std::vector<FaviconBitmapResult>* favicon_bitmap_results);

  // Build mapping of the icon URLs for |favicon_ids| to the sizes of the
  // favicon bitmaps available at each icon URL on the web. Favicon bitmaps
  // might not be cached in the thumbnail database for any of the sizes in the
  // returned map. See history_types.h for a more detailed description of
  // IconURLSizesMap.
  // Returns true if map was successfully built.
  bool BuildIconURLSizesMap(const std::vector<FaviconID>& favicon_ids,
                            IconURLSizesMap* icon_url_sizes);

  // Maps the favicon ids in |icon_ids| to |page_url| (and all redirects)
  // for |icon_type|.
  // Returns true if the mappings for the page or any of its redirects were
  // changed.
  bool SetFaviconMappingsForPageAndRedirects(
      const GURL& page_url,
      IconType icon_type,
      const std::vector<FaviconID>& icon_ids);

  // Maps the favicon ids in |icon_ids| to |page_url| for |icon_type|.
  // Returns true if the function changed some of |page_url|'s mappings.
  bool SetFaviconMappingsForPage(const GURL& page_url,
                                 IconType icon_type,
                                 const std::vector<FaviconID>& icon_ids);

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

  // Schedules a broadcast of the given notification on the main thread. The
  // details argument will have ownership taken by this function (it will be
  // sent to the main thread and deleted there).
  virtual void BroadcastNotifications(int type,
                                      HistoryDetails* details_deleted) OVERRIDE;

  // Deleting all history ------------------------------------------------------

  // Deletes all history. This is a special case of deleting that is separated
  // from our normal dependency-following method for performance reasons. The
  // logic lives here instead of ExpireHistoryBackend since it will cause
  // re-initialization of some databases such as Thumbnails or Archived that
  // could fail. When these databases are not valid, our pointers must be NULL,
  // so we need to handle this type of operation to keep the pointers in sync.
  void DeleteAllHistory();

  // Given a vector of all URLs that we will keep, removes all thumbnails
  // referenced by any URL, and also all favicons that aren't used by those
  // URLs. The favicon IDs will change, so this will update the url rows in the
  // vector to reference the new IDs.
  bool ClearAllThumbnailHistory(URLRows* kept_urls);

  // Deletes all information in the history database, except for the supplied
  // set of URLs in the URL table (these should correspond to the bookmarked
  // URLs).
  //
  // The IDs of the URLs may change.
  bool ClearAllMainHistory(const URLRows& kept_urls);

  // Returns the BookmarkService, blocking until it is loaded. This may return
  // NULL during testing.
  BookmarkService* GetBookmarkService();

  // Notify any observers of an addition to the visit database.
  void NotifyVisitObservers(const VisitRow& visit);

  // Data ----------------------------------------------------------------------

  // Delegate. See the class definition above for more information. This will
  // be NULL before Init is called and after Cleanup, but is guaranteed
  // non-NULL in between.
  scoped_ptr<Delegate> delegate_;

  // The id of this class, given in creation and used for identifying the
  // backend when calling the delegate.
  int id_;

  // Directory where database files will be stored.
  FilePath history_dir_;

  // The history/thumbnail databases. Either MAY BE NULL if the database could
  // not be opened, all users must first check for NULL and return immediately
  // if it is. The thumbnail DB may be NULL when the history one isn't, but not
  // vice-versa.
  scoped_ptr<HistoryDatabase> db_;
  scoped_ptr<ThumbnailDatabase> thumbnail_db_;

  // Stores old history in a larger, slower database.
  scoped_ptr<ArchivedDatabase> archived_db_;

  // Full text database manager, possibly NULL if the database could not be
  // created.
  scoped_ptr<TextDatabaseManager> text_database_;

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
  MessageLoop* backend_destroy_message_loop_;
  base::Closure backend_destroy_task_;

  // Tracks page transition types.
  VisitTracker tracker_;

  // A boolean variable to track whether we have already purged obsolete segment
  // data.
  bool segment_queried_;

  // HistoryDBTasks to run. Be sure to AddRef when adding, and Release when
  // done.
  std::list<HistoryDBTaskRequest*> db_task_requests_;

  // Used to determine if a URL is bookmarked. This is owned by the Profile and
  // may be NULL (during testing).
  //
  // Use GetBookmarkService to access this, which makes sure the service is
  // loaded.
  BookmarkService* bookmark_service_;

  // Publishes the history to all indexers which are registered to receive
  // history data from us. Can be NULL if there are no listeners.
  scoped_ptr<HistoryPublisher> history_publisher_;

#if defined(OS_ANDROID)
  // Used to provide the Android ContentProvider APIs.
  scoped_ptr<AndroidProviderBackend> android_provider_backend_;
#endif

  DISALLOW_COPY_AND_ASSIGN(HistoryBackend);
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_BACKEND_H_
