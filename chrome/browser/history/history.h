// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_H_
#define CHROME_BROWSER_HISTORY_HISTORY_H_

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/browser/visitedlink/visitedlink_delegate.h"
#include "chrome/common/cancelable_task_tracker.h"
#include "chrome/common/ref_counted_util.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/page_transition_types.h"
#include "sql/init_status.h"
#include "sync/api/syncable_service.h"
#include "ui/base/layout.h"

#if defined(OS_ANDROID)
#include "chrome/browser/history/android/android_history_provider_service.h"
#endif

class BookmarkService;
class FilePath;
class GURL;
class HistoryURLProvider;
class PageUsageData;
class PageUsageRequest;
class Profile;
class VisitedLinkMaster;
struct HistoryURLProviderParams;

namespace base {
class Thread;
}


namespace history {

class HistoryBackend;
class HistoryDatabase;
class HistoryQueryTest;
class InMemoryHistoryBackend;
class InMemoryURLIndex;
class InMemoryURLIndexTest;
class URLDatabase;
class VisitDatabaseObserver;
class VisitFilter;
struct DownloadRow;
struct HistoryAddPageArgs;
struct HistoryDetails;

}  // namespace history

namespace sync_pb {
class HistoryDeleteDirectiveSpecifics;
}

// HistoryDBTask can be used to process arbitrary work on the history backend
// thread. HistoryDBTask is scheduled using HistoryService::ScheduleDBTask.
// When HistoryBackend processes the task it invokes RunOnDBThread. Once the
// task completes and has not been canceled, DoneRunOnMainThread is invoked back
// on the main thread.
class HistoryDBTask : public base::RefCountedThreadSafe<HistoryDBTask> {
 public:
  // Invoked on the database thread. The return value indicates whether the
  // task is done. A return value of true signals the task is done and
  // RunOnDBThread should NOT be invoked again. A return value of false
  // indicates the task is not done, and should be run again after other
  // tasks are given a chance to be processed.
  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) = 0;

  // Invoked on the main thread once RunOnDBThread has returned true. This is
  // only invoked if the request was not canceled and returned true from
  // RunOnDBThread.
  virtual void DoneRunOnMainThread() = 0;

 protected:
  friend class base::RefCountedThreadSafe<HistoryDBTask>;

  virtual ~HistoryDBTask() {}
};

// The history service records page titles, and visit times, as well as
// (eventually) information about autocomplete.
//
// This service is thread safe. Each request callback is invoked in the
// thread that made the request.
class HistoryService : public CancelableRequestProvider,
                       public content::NotificationObserver,
                       public syncer::SyncableService,
                       public ProfileKeyedService,
                       public VisitedLinkDelegate {
 public:
  // Miscellaneous commonly-used types.
  typedef std::vector<PageUsageData*> PageUsageDataList;

  // Must call Init after construction.
  explicit HistoryService(Profile* profile);
  // The empty constructor is provided only for testing.
  HistoryService();

  virtual ~HistoryService();

  // Initializes the history service, returning true on success. On false, do
  // not call any other functions. The given directory will be used for storing
  // the history files. The BookmarkService is used when deleting URLs to
  // test if a URL is bookmarked; it may be NULL during testing.
  bool Init(const FilePath& history_dir, BookmarkService* bookmark_service) {
    return Init(history_dir, bookmark_service, false);
  }

  // Triggers the backend to load if it hasn't already, and then returns whether
  // it's finished loading.
  // Note: Virtual needed for mocking.
  virtual bool BackendLoaded();

  // Returns true if the backend has finished loading.
  bool backend_loaded() const { return backend_loaded_; }

  // Unloads the backend without actually shutting down the history service.
  // This can be used to temporarily reduce the browser process' memory
  // footprint.
  void UnloadBackend();

  // Called on shutdown, this will tell the history backend to complete and
  // will release pointers to it. No other functions should be called once
  // cleanup has happened that may dispatch to the history thread (because it
  // will be NULL).
  //
  // In practice, this will be called by the service manager (BrowserProcess)
  // when it is being destroyed. Because that reference is being destroyed, it
  // should be impossible for anybody else to call the service, even if it is
  // still in memory (pending requests may be holding a reference to us).
  void Cleanup();

  // RenderProcessHost pointers are used to scope page IDs (see AddPage). These
  // objects must tell us when they are being destroyed so that we can clear
  // out any cached data associated with that scope.
  //
  // The given pointer will not be dereferenced, it is only used for
  // identification purposes, hence it is a void*.
  void NotifyRenderProcessHostDestruction(const void* host);

  // Triggers the backend to load if it hasn't already, and then returns the
  // in-memory URL database. The returned pointer MAY BE NULL if the in-memory
  // database has not been loaded yet. This pointer is owned by the history
  // system. Callers should not store or cache this value.
  //
  // TODO(brettw) this should return the InMemoryHistoryBackend.
  history::URLDatabase* InMemoryDatabase();

  // Following functions get URL information from in-memory database.
  // They return false if database is not available (e.g. not loaded yet) or the
  // URL does not exist.

  // Reads the number of times the user has typed the given URL.
  bool GetTypedCountForURL(const GURL& url, int* typed_count);

  // Reads the last visit time for the given URL.
  bool GetLastVisitTimeForURL(const GURL& url, base::Time* last_visit);

  // Reads the number of times this URL has been visited.
  bool GetVisitCountForURL(const GURL& url, int* visit_count);

  // Return the quick history index.
  history::InMemoryURLIndex* InMemoryIndex() const {
    return in_memory_url_index_.get();
  }

  // ProfileKeyedService:
  virtual void Shutdown() OVERRIDE;

  // Navigation ----------------------------------------------------------------

  // Adds the given canonical URL to history with the given time as the visit
  // time. Referrer may be the empty string.
  //
  // The supplied render process host is used to scope the given page ID. Page
  // IDs are only unique inside a given render process, so we need that to
  // differentiate them. This pointer should not be dereferenced by the history
  // system.
  //
  // The scope/ids can be NULL if there is no meaningful tracking information
  // that can be performed on the given URL. The 'page_id' should be the ID of
  // the current session history entry in the given process.
  //
  // 'redirects' is an array of redirect URLs leading to this page, with the
  // page itself as the last item (so when there is no redirect, it will have
  // one entry). If there are no redirects, this array may also be empty for
  // the convenience of callers.
  //
  // 'did_replace_entry' is true when the navigation entry for this page has
  // replaced the existing entry. A non-user initiated redirect causes such
  // replacement.
  //
  // All "Add Page" functions will update the visited link database.
  void AddPage(const GURL& url,
               base::Time time,
               const void* id_scope,
               int32 page_id,
               const GURL& referrer,
               const history::RedirectList& redirects,
               content::PageTransition transition,
               history::VisitSource visit_source,
               bool did_replace_entry);

  // For adding pages to history where no tracking information can be done.
  void AddPage(const GURL& url,
               base::Time time,
               history::VisitSource visit_source);

  // All AddPage variants end up here.
  void AddPage(const history::HistoryAddPageArgs& add_page_args);

  // Adds an entry for the specified url without creating a visit. This should
  // only be used when bookmarking a page, otherwise the row leaks in the
  // history db (it never gets cleaned).
  void AddPageNoVisitForBookmark(const GURL& url, const string16& title);

  // Sets the title for the given page. The page should be in history. If it
  // is not, this operation is ignored. This call will not update the full
  // text index. The last title set when the page is indexed will be the
  // title in the full text index.
  void SetPageTitle(const GURL& url, const string16& title);

  // Updates the history database with a page's ending time stamp information.
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

  // Notifies history of the body text of the given recently-visited URL.
  // If the URL was not visited "recently enough," the history system may
  // discard it.
  void SetPageContents(const GURL& url, const string16& contents);

  // Querying ------------------------------------------------------------------

  // Returns the information about the requested URL. If the URL is found,
  // success will be true and the information will be in the URLRow parameter.
  // On success, the visits, if requested, will be sorted by date. If they have
  // not been requested, the pointer will be valid, but the vector will be
  // empty.
  //
  // If success is false, neither the row nor the vector will be valid.
  typedef base::Callback<void(
      Handle,
      bool,  // Success flag, when false, nothing else is valid.
      const history::URLRow*,
      history::VisitVector*)> QueryURLCallback;

  // Queries the basic information about the URL in the history database. If
  // the caller is interested in the visits (each time the URL is visited),
  // set |want_visits| to true. If these are not needed, the function will be
  // faster by setting this to false.
  Handle QueryURL(const GURL& url,
                  bool want_visits,
                  CancelableRequestConsumerBase* consumer,
                  const QueryURLCallback& callback);

  // Provides the result of a query. See QueryResults in history_types.h.
  // The common use will be to use QueryResults.Swap to suck the contents of
  // the results out of the passed in parameter and take ownership of them.
  typedef base::Callback<void(Handle, history::QueryResults*)>
      QueryHistoryCallback;

  // Queries all history with the given options (see QueryOptions in
  // history_types.h). If non-empty, the full-text database will be queried with
  // the given |text_query|. If empty, all results matching the given options
  // will be returned.
  //
  // This isn't totally hooked up yet, this will query the "new" full text
  // database (see SetPageContents) which won't generally be set yet.
  Handle QueryHistory(const string16& text_query,
                      const history::QueryOptions& options,
                      CancelableRequestConsumerBase* consumer,
                      const QueryHistoryCallback& callback);

  // Called when the results of QueryRedirectsFrom are available.
  // The given vector will contain a list of all redirects, not counting
  // the original page. If A redirects to B, the vector will contain only B,
  // and A will be in 'source_url'.
  //
  // If there is no such URL in the database or the most recent visit has no
  // redirect, the vector will be empty. If the history system failed for
  // some reason, success will additionally be false. If the given page
  // has redirected to multiple destinations, this will pick a random one.
  typedef base::Callback<void(Handle,
                              GURL,  // from_url
                              bool,  // success
                              history::RedirectList*)> QueryRedirectsCallback;

  // Schedules a query for the most recent redirect coming out of the given
  // URL. See the RedirectQuerySource above, which is guaranteed to be called
  // if the request is not canceled.
  Handle QueryRedirectsFrom(const GURL& from_url,
                            CancelableRequestConsumerBase* consumer,
                            const QueryRedirectsCallback& callback);

  // Schedules a query to get the most recent redirects ending at the given
  // URL.
  Handle QueryRedirectsTo(const GURL& to_url,
                          CancelableRequestConsumerBase* consumer,
                          const QueryRedirectsCallback& callback);

  typedef base::Callback<
      void(Handle,
           bool,        // Were we able to determine the # of visits?
           int,         // Number of visits.
           base::Time)> // Time of first visit. Only set if bool
                        // is true and int is > 0.
      GetVisibleVisitCountToHostCallback;

  // Requests the number of user-visible visits (i.e. no redirects or subframes)
  // to all urls on the same scheme/host/port as |url|.  This is only valid for
  // HTTP and HTTPS URLs.
  Handle GetVisibleVisitCountToHost(
      const GURL& url,
      CancelableRequestConsumerBase* consumer,
      const GetVisibleVisitCountToHostCallback& callback);

  // Called when QueryTopURLsAndRedirects completes. The vector contains a list
  // of the top |result_count| URLs.  For each of these URLs, there is an entry
  // in the map containing redirects from the URL.  For example, if we have the
  // redirect chain A -> B -> C and A is a top visited URL, then A will be in
  // the vector and "A => {B -> C}" will be in the map.
  typedef base::Callback<
      void(Handle,
           bool,  // Did we get the top urls and redirects?
           std::vector<GURL>*,  // List of top URLs.
           history::RedirectMap*)>  // Redirects for top URLs.
      QueryTopURLsAndRedirectsCallback;

  // Request the top |result_count| most visited URLs and the chain of redirects
  // leading to each of these URLs.
  // TODO(Nik): remove this. Use QueryMostVisitedURLs instead.
  Handle QueryTopURLsAndRedirects(
      int result_count,
      CancelableRequestConsumerBase* consumer,
      const QueryTopURLsAndRedirectsCallback& callback);

  typedef base::Callback<void(Handle, history::MostVisitedURLList)>
      QueryMostVisitedURLsCallback;

  typedef base::Callback<void(Handle, const history::FilteredURLList&)>
      QueryFilteredURLsCallback;

  // Request the |result_count| most visited URLs and the chain of
  // redirects leading to each of these URLs. |days_back| is the
  // number of days of history to use. Used by TopSites.
  Handle QueryMostVisitedURLs(int result_count, int days_back,
                              CancelableRequestConsumerBase* consumer,
                              const QueryMostVisitedURLsCallback& callback);

  // Request the |result_count| URLs filtered and sorted based on the |filter|.
  // If |extended_info| is true, additional data will be provided in the
  // results. Computing this additional data is expensive, likely to become
  // more expensive as additional data points are added in future changes, and
  // not useful in most cases. Set |extended_info| to true only if you
  // explicitly require the additional data.
  Handle QueryFilteredURLs(
      int result_count,
      const history::VisitFilter& filter,
      bool extended_info,
      CancelableRequestConsumerBase* consumer,
      const QueryFilteredURLsCallback& callback);

  // Thumbnails ----------------------------------------------------------------

  // Implemented by consumers to get thumbnail data. Called when a request for
  // the thumbnail data is complete. Once this callback is made, the request
  // will be completed and no other calls will be made for that handle.
  //
  // This function will be called even on error conditions or if there is no
  // thumbnail for that page. In these cases, the data pointer will be NULL.
  typedef base::Callback<void(Handle, scoped_refptr<base::RefCountedBytes>)>
      ThumbnailDataCallback;

  // Requests a page thumbnail. See ThumbnailDataCallback definition above.
  Handle GetPageThumbnail(const GURL& page_url,
                          CancelableRequestConsumerBase* consumer,
                          const ThumbnailDataCallback& callback);

  // Database management operations --------------------------------------------

  // Delete all the information related to a single url.
  void DeleteURL(const GURL& url);

  // Delete all the information related to a list of urls.  (Deleting
  // URLs one by one is slow as it has to flush to disk each time.)
  void DeleteURLsForTest(const std::vector<GURL>& urls);

  // Removes all visits in the selected time range (including the start time),
  // updating the URLs accordingly. This deletes the associated data, including
  // the full text index. This function also deletes the associated favicons,
  // if they are no longer referenced. |callback| runs when the expiration is
  // complete. You may use null Time values to do an unbounded delete in
  // either direction.
  // If |restrict_urls| is not empty, only visits to the URLs in this set are
  // removed.
  void ExpireHistoryBetween(const std::set<GURL>& restrict_urls,
                            base::Time begin_time,
                            base::Time end_time,
                            const base::Closure& callback,
                            CancelableTaskTracker* tracker);

  // Downloads -----------------------------------------------------------------

  // Implemented by the caller of 'CreateDownload' below, and is called when the
  // history service has created a new entry for a download in the history db.
  typedef base::Callback<void(int64)> DownloadCreateCallback;

  // Begins a history request to create a new row for a download. 'info'
  // contains all the download's creation state, and 'callback' runs when the
  // history service request is complete. The callback is called on the thread
  // that calls CreateDownload().
  void CreateDownload(
      const history::DownloadRow& info,
      const DownloadCreateCallback& callback);

  // Implemented by the caller of 'GetNextDownloadId' below.
  typedef base::Callback<void(int)> DownloadNextIdCallback;

  // Runs the callback with the next available download id. The callback is
  // called on the thread that calls GetNextDownloadId().
  void GetNextDownloadId(const DownloadNextIdCallback& callback);

  // Implemented by the caller of 'QueryDownloads' below, and is called when the
  // history service has retrieved a list of all download state. The call
  typedef base::Callback<void(
      scoped_ptr<std::vector<history::DownloadRow> >)>
          DownloadQueryCallback;

  // Begins a history request to retrieve the state of all downloads in the
  // history db. 'callback' runs when the history service request is complete,
  // at which point 'info' contains an array of history::DownloadRow, one per
  // download. The callback is called on the thread that calls QueryDownloads().
  void QueryDownloads(const DownloadQueryCallback& callback);

  // Begins a request to clean up entries that has been corrupted (because of
  // the crash, for example).
  void CleanUpInProgressEntries();

  // Called to update the history service about the current state of a download.
  // This is a 'fire and forget' query, so just pass the relevant state info to
  // the database with no need for a callback.
  void UpdateDownload(const history::DownloadRow& data);

  // Permanently remove some downloads from the history system. This is a 'fire
  // and forget' operation.
  void RemoveDownloads(const std::set<int64>& db_handles);

  // Visit Segments ------------------------------------------------------------

  typedef base::Callback<void(Handle, std::vector<PageUsageData*>*)>
      SegmentQueryCallback;

  // Query usage data for all visit segments since the provided time.
  //
  // The request is performed asynchronously and can be cancelled by using the
  // returned handle.
  //
  // The vector provided to the callback and its contents is owned by the
  // history system. It will be deeply deleted after the callback is invoked.
  // If you want to preserve any PageUsageData instance, simply remove them
  // from the vector.
  //
  // The vector contains a list of PageUsageData. Each PageUsageData ID is set
  // to the segment ID. The URL and all the other information is set to the page
  // representing the segment.
  Handle QuerySegmentUsageSince(CancelableRequestConsumerBase* consumer,
                                const base::Time from_time,
                                int max_result_count,
                                const SegmentQueryCallback& callback);

  // Set the presentation index for the segment identified by |segment_id|.
  void SetSegmentPresentationIndex(int64 segment_id, int index);

  // Keyword search terms -----------------------------------------------------

  // Sets the search terms for the specified url and keyword. url_id gives the
  // id of the url, keyword_id the id of the keyword and term the search term.
  void SetKeywordSearchTermsForURL(const GURL& url,
                                   TemplateURLID keyword_id,
                                   const string16& term);

  // Deletes all search terms for the specified keyword.
  void DeleteAllSearchTermsForKeyword(TemplateURLID keyword_id);

  typedef base::Callback<
      void(Handle, std::vector<history::KeywordSearchTermVisit>*)>
          GetMostRecentKeywordSearchTermsCallback;

  // Returns up to max_count of the most recent search terms starting with the
  // specified text. The matching is case insensitive. The results are ordered
  // in descending order up to |max_count| with the most recent search term
  // first.
  Handle GetMostRecentKeywordSearchTerms(
      TemplateURLID keyword_id,
      const string16& prefix,
      int max_count,
      CancelableRequestConsumerBase* consumer,
      const GetMostRecentKeywordSearchTermsCallback& callback);

  // Bookmarks -----------------------------------------------------------------

  // Notification that a URL is no longer bookmarked.
  void URLsNoLongerBookmarked(const std::set<GURL>& urls);

  // Generic Stuff -------------------------------------------------------------

  // Schedules a HistoryDBTask for running on the history backend thread. See
  // HistoryDBTask for details on what this does.
  virtual void ScheduleDBTask(HistoryDBTask* task,
                              CancelableRequestConsumerBase* consumer);

  // Returns true if top sites needs to be migrated out of history into its own
  // db.
  bool needs_top_sites_migration() const { return needs_top_sites_migration_; }

  // Adds or removes observers for the VisitDatabase.
  void AddVisitDatabaseObserver(history::VisitDatabaseObserver* observer);
  void RemoveVisitDatabaseObserver(history::VisitDatabaseObserver* observer);

  void NotifyVisitDBObserversOnAddVisit(const history::BriefVisitInfo& info);

  // Testing -------------------------------------------------------------------

  // Designed for unit tests, this passes the given task on to the history
  // backend to be called once the history backend has terminated. This allows
  // callers to know when the history thread is complete and the database files
  // can be deleted and the next test run. Otherwise, the history thread may
  // still be running, causing problems in subsequent tests.
  //
  // There can be only one closing task, so this will override any previously
  // set task. We will take ownership of the pointer and delete it when done.
  // The task will be run on the calling thread (this function is threadsafe).
  void SetOnBackendDestroyTask(const base::Closure& task);

  // Used for unit testing and potentially importing to get known information
  // into the database. This assumes the URL doesn't exist in the database
  //
  // Calling this function many times may be slow because each call will
  // dispatch to the history thread and will be a separate database
  // transaction. If this functionality is needed for importing many URLs,
  // callers should use AddPagesWithDetails() instead.
  //
  // Note that this routine (and AddPageWithDetails()) always adds a single
  // visit using the |last_visit| timestamp, and a PageTransition type of LINK,
  // if |visit_source| != SYNCED.
  void AddPageWithDetails(const GURL& url,
                          const string16& title,
                          int visit_count,
                          int typed_count,
                          base::Time last_visit,
                          bool hidden,
                          history::VisitSource visit_source);

  // The same as AddPageWithDetails() but takes a vector.
  void AddPagesWithDetails(const history::URLRows& info,
                           history::VisitSource visit_source);

  // Starts the TopSites migration in the HistoryThread. Called by the
  // BackendDelegate.
  void StartTopSitesMigration(int backend_id);

  // Called by TopSites after the thumbnails were read and it is safe
  // to delete the thumbnails DB.
  void OnTopSitesReady();

  // Returns true if this looks like the type of URL we want to add to the
  // history. We filter out some URLs such as JavaScript.
  static bool CanAddURL(const GURL& url);

  base::WeakPtr<HistoryService> AsWeakPtr();

  void ProcessDeleteDirectiveForTest(
      const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive);

  // syncer::SyncableService implementation.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // Processes the given |delete_directive| and sends it to the
  // SyncChangeProcessor (if it exists).  Returns any error resulting
  // from sending the delete directive to sync.
  syncer::SyncError ProcessLocalDeleteDirective(
      const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive);

 protected:
  // These are not currently used, hopefully we can do something in the future
  // to ensure that the most important things happen first.
  enum SchedulePriority {
    PRIORITY_UI,      // The highest priority (must respond to UI events).
    PRIORITY_NORMAL,  // Normal stuff like adding a page.
    PRIORITY_LOW,     // Low priority things like indexing or expiration.
  };

 private:
  class BackendDelegate;
#if defined(OS_ANDROID)
  friend class AndroidHistoryProviderService;
#endif
  friend class base::RefCountedThreadSafe<HistoryService>;
  friend class BackendDelegate;
  friend class FaviconService;
  friend class history::HistoryBackend;
  friend class history::HistoryQueryTest;
  friend class HistoryOperation;
  friend class HistoryURLProvider;
  friend class HistoryURLProviderTest;
  friend class history::InMemoryURLIndexTest;
  template<typename Info, typename Callback> friend class DownloadRequest;
  friend class PageUsageRequest;
  friend class RedirectRequest;
  friend class TestingProfile;

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Implementation of VisitedLinkDelegate.
  virtual bool AreEquivalentContexts(
      content::BrowserContext* context1,
      content::BrowserContext* context2) OVERRIDE;
  virtual void RebuildTable(
      const scoped_refptr<URLEnumerator>& enumerator) OVERRIDE;

  // Low-level Init().  Same as the public version, but adds a |no_db| parameter
  // that is only set by unittests which causes the backend to not init its DB.
  bool Init(const FilePath& history_dir,
            BookmarkService* bookmark_service,
            bool no_db);

  // Called by the HistoryURLProvider class to schedule an autocomplete, it
  // will be called back on the internal history thread with the history
  // database so it can query. See history_autocomplete.cc for a diagram.
  void ScheduleAutocomplete(HistoryURLProvider* provider,
                            HistoryURLProviderParams* params);

  // Broadcasts the given notification. This is called by the backend so that
  // the notification will be broadcast on the main thread.
  //
  // Compared to BroadcastNotifications(), this function does not take
  // ownership of |details|.
  void BroadcastNotificationsHelper(int type,
                                    history::HistoryDetails* details);

  // Initializes the backend.
  void LoadBackendIfNecessary();

  // Notification from the backend that it has finished loading. Sends
  // notification (NOTIFY_HISTORY_LOADED) and sets backend_loaded_ to true.
  void OnDBLoaded(int backend_id);

  // Helper function for getting URL information.
  // Reads a URLRow from in-memory database. Returns false if database is not
  // available or the URL does not exist.
  bool GetRowForURL(const GURL& url, history::URLRow* url_row);

  // Favicon -------------------------------------------------------------------

  // These favicon methods are exposed to the FaviconService. Instead of calling
  // these methods directly you should call the respective method on the
  // FaviconService.

  // Used by FaviconService to get the favicon bitmaps from the history backend
  // which most closely match |desired_size_in_dip| x |desired_size_in_dip| and
  // |desired_scale_factors| for |icon_types|. If |desired_size_in_dip| is 0,
  // the largest favicon bitmap for |icon_types| is returned. The returned
  // FaviconBitmapResults will have at most one result for each of
  // |desired_scale_factors|. If a favicon bitmap is determined to be the best
  // candidate for multiple scale factors there will be less results.
  // If |icon_types| has several types, results for only a single type will be
  // returned in the priority of TOUCH_PRECOMPOSED_ICON, TOUCH_ICON, and
  // FAVICON.
  CancelableTaskTracker::TaskId GetFavicons(
      const std::vector<GURL>& icon_urls,
      int icon_types,
      int desired_size_in_dip,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      const FaviconService::FaviconResultsCallback& callback,
      CancelableTaskTracker* tracker);

  // Used by the FaviconService to get favicons mapped to |page_url| for
  // |icon_types| which most closely match |desired_size_in_dip| and
  // |desired_scale_factors|. If |desired_size_in_dip| is 0, the largest favicon
  // bitmap for |icon_types| is returned. The returned FaviconBitmapResults will
  // have at most one result for each of |desired_scale_factors|. If a favicon
  // bitmap is determined to be the best candidate for multiple scale factors
  // there will be less results. If |icon_types| has several types, results for
  // only a single type will be returned in the priority of
  // TOUCH_PRECOMPOSED_ICON, TOUCH_ICON, and FAVICON.
  CancelableTaskTracker::TaskId GetFaviconsForURL(
      const GURL& page_url,
      int icon_types,
      int desired_size_in_dip,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      const FaviconService::FaviconResultsCallback& callback,
      CancelableTaskTracker* tracker);

  // Used by the FaviconService to get the favicon bitmap which most closely
  // matches |desired_size_in_dip| and |desired_scale_factor| from the favicon
  // with |favicon_id| from the history backend. If |desired_size_in_dip| is 0,
  // the largest favicon bitmap for |favicon_id| is returned.
  CancelableTaskTracker::TaskId GetFaviconForID(
      history::FaviconID favicon_id,
      int desired_size_in_dip,
      ui::ScaleFactor desired_scale_factor,
      const FaviconService::FaviconResultsCallback& callback,
      CancelableTaskTracker* tracker);

  // Used by the FaviconService to replace the favicon mappings to |page_url|
  // for |icon_types| on the history backend.
  // Sample |icon_urls|:
  //  { ICON_URL1 -> TOUCH_ICON, known to the database,
  //    ICON_URL2 -> TOUCH_ICON, not known to the database,
  //    ICON_URL3 -> TOUCH_PRECOMPOSED_ICON, known to the database }
  // The new mappings are computed from |icon_urls| with these rules:
  // 1) Any urls in |icon_urls| which are not already known to the database are
  //    rejected.
  //    Sample new mappings to |page_url|: { ICON_URL1, ICON_URL3 }
  // 2) If |icon_types| has multiple types, the mappings are only set for the
  //    largest icon type.
  //    Sample new mappings to |page_url|: { ICON_URL3 }
  // |icon_types| can only have multiple IconTypes if
  // |icon_types| == TOUCH_ICON | TOUCH_PRECOMPOSED_ICON.
  // The favicon bitmaps which most closely match |desired_size_in_dip|
  // and |desired_scale_factors| from the favicons which were just mapped
  // to |page_url| are returned. If |desired_size_in_dip| is 0, the
  // largest favicon bitmap is returned.
  CancelableTaskTracker::TaskId UpdateFaviconMappingsAndFetch(
      const GURL& page_url,
      const std::vector<GURL>& icon_urls,
      int icon_types,
      int desired_size_in_dip,
      const std::vector<ui::ScaleFactor>& desired_scale_factors,
      const FaviconService::FaviconResultsCallback& callback,
      CancelableTaskTracker* tracker);

  // Used by FaviconService to set a favicon for |page_url| and |icon_url| with
  // |pixel_size|.
  // Example:
  //   |page_url|: www.google.com
  // 2 favicons in history for |page_url|:
  //   www.google.com/a.ico  16x16
  //   www.google.com/b.ico  32x32
  // MergeFavicon(|page_url|, www.google.com/a.ico, ..., ..., 16x16)
  //
  // Merging occurs in the following manner:
  // 1) |page_url| is set to map to only to |icon_url|. In order to not lose
  //    data, favicon bitmaps mapped to |page_url| but not to |icon_url| are
  //    copied to the favicon at |icon_url|.
  //    For the example above, |page_url| will only be mapped to a.ico.
  //    The 32x32 favicon bitmap at b.ico is copied to a.ico
  // 2) |bitmap_data| is added to the favicon at |icon_url|, overwriting any
  //    favicon bitmaps of |pixel_size|.
  //    For the example above, |bitmap_data| overwrites the 16x16 favicon
  //    bitmap for a.ico.
  // TODO(pkotwicz): Remove once no longer required by sync.
  void MergeFavicon(const GURL& page_url,
                    const GURL& icon_url,
                    history::IconType icon_type,
                    scoped_refptr<base::RefCountedMemory> bitmap_data,
                    const gfx::Size& pixel_size);

  // Used by the FaviconService to set the favicons for a page on the history
  // backend.
  // |favicon_bitmap_data| is a listing of additional favicon bitmaps to store
  // for |page_url|.
  // |expired| and |icon_type| fields in FaviconBitmapData are ignored.
  // |icon_url_sizes| is a mapping of all the icon urls of the favicons
  // available for |page_url| to the sizes that those favicons are available
  // from the web.
  // |favicon_bitmap_data| does not need to have entries for all the icon urls
  // or sizes listed in |icon_url_sizes|. However, the icon urls and pixel
  // sizes in |favicon_bitmap_data| must be a subset of |icon_url_sizes|. It is
  // important that |icon_url_sizes| be complete as mappings to favicons whose
  // icon url or pixel size is not in |icon_url_sizes| will be deleted.
  // Use MergeFavicon() if any of the icon URLs for |page_url| or any of the
  // favicon sizes of the icon URLs are not known.
  // See HistoryBackend::ValidateSetFaviconsParams() for more details on the
  // criteria for |favicon_bitmap_data| and |icon_url_sizes| to be valid.
  void SetFavicons(
      const GURL& page_url,
      history::IconType icon_type,
      const std::vector<history::FaviconBitmapData>& favicon_bitmap_data,
      const history::IconURLSizesMap& icon_url_sizes);

  // Used by the FaviconService to mark the favicon for the page as being out
  // of date.
  void SetFaviconsOutOfDateForPage(const GURL& page_url);

  // Used by the FaviconService to clone favicons from one page to another,
  // provided that other page does not already have favicons.
  void CloneFavicons(const GURL& old_page_url, const GURL& new_page_url);

  // Used by the FaviconService for importing many favicons for many pages at
  // once. The pages must exist, any favicon sets for unknown pages will be
  // discarded. Existing favicons will not be overwritten.
  void SetImportedFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicon_usage);

  // Sets the in-memory URL database. This is called by the backend once the
  // database is loaded to make it available.
  void SetInMemoryBackend(int backend_id,
                          history::InMemoryHistoryBackend* mem_backend);

  // Called by our BackendDelegate when there is a problem reading the database.
  void NotifyProfileError(int backend_id, sql::InitStatus init_status);

  // Call to schedule a given task for running on the history thread with the
  // specified priority. The task will have ownership taken.
  void ScheduleTask(SchedulePriority priority, const base::Closure& task);

  // Delete local history according to the given directive (from
  // sync).
  void ProcessDeleteDirective(
      const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive);

  // Called when a delete directive has been processed.
  void OnDeleteDirectiveProcessed(
      const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive);

  // Schedule ------------------------------------------------------------------
  //
  // Functions for scheduling operations on the history thread that have a
  // handle and may be cancelable. For fire-and-forget operations, see
  // ScheduleAndForget below.

  template<typename BackendFunc, class RequestType>
  Handle Schedule(SchedulePriority priority,
                  BackendFunc func,  // Function to call on the HistoryBackend.
                  CancelableRequestConsumerBase* consumer,
                  RequestType* request) {
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    if (consumer)
      AddRequest(request, consumer);
    ScheduleTask(priority,
                 base::Bind(func, history_backend_.get(),
                            scoped_refptr<RequestType>(request)));
    return request->handle();
  }

  template<typename BackendFunc, class RequestType, typename ArgA>
  Handle Schedule(SchedulePriority priority,
                  BackendFunc func,  // Function to call on the HistoryBackend.
                  CancelableRequestConsumerBase* consumer,
                  RequestType* request,
                  const ArgA& a) {
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    if (consumer)
      AddRequest(request, consumer);
    ScheduleTask(priority,
                 base::Bind(func, history_backend_.get(),
                            scoped_refptr<RequestType>(request), a));
    return request->handle();
  }

  template<typename BackendFunc,
           class RequestType,  // Descendant of CancelableRequestBase.
           typename ArgA,
           typename ArgB>
  Handle Schedule(SchedulePriority priority,
                  BackendFunc func,  // Function to call on the HistoryBackend.
                  CancelableRequestConsumerBase* consumer,
                  RequestType* request,
                  const ArgA& a,
                  const ArgB& b) {
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    if (consumer)
      AddRequest(request, consumer);
    ScheduleTask(priority,
                 base::Bind(func, history_backend_.get(),
                            scoped_refptr<RequestType>(request), a, b));
    return request->handle();
  }

  template<typename BackendFunc,
           class RequestType,  // Descendant of CancelableRequestBase.
           typename ArgA,
           typename ArgB,
           typename ArgC>
  Handle Schedule(SchedulePriority priority,
                  BackendFunc func,  // Function to call on the HistoryBackend.
                  CancelableRequestConsumerBase* consumer,
                  RequestType* request,
                  const ArgA& a,
                  const ArgB& b,
                  const ArgC& c) {
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    if (consumer)
      AddRequest(request, consumer);
    ScheduleTask(priority,
                 base::Bind(func, history_backend_.get(),
                            scoped_refptr<RequestType>(request), a, b, c));
    return request->handle();
  }

  template<typename BackendFunc,
           class RequestType,  // Descendant of CancelableRequestBase.
           typename ArgA,
           typename ArgB,
           typename ArgC,
           typename ArgD>
  Handle Schedule(SchedulePriority priority,
                  BackendFunc func,  // Function to call on the HistoryBackend.
                  CancelableRequestConsumerBase* consumer,
                  RequestType* request,
                  const ArgA& a,
                  const ArgB& b,
                  const ArgC& c,
                  const ArgD& d) {
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    if (consumer)
      AddRequest(request, consumer);
    ScheduleTask(priority,
                 base::Bind(func, history_backend_.get(),
                            scoped_refptr<RequestType>(request), a, b, c, d));
    return request->handle();
  }

  // ScheduleAndForget ---------------------------------------------------------
  //
  // Functions for scheduling operations on the history thread that do not need
  // any callbacks and are not cancelable.

  template<typename BackendFunc>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func) {  // Function to call on backend.
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    ScheduleTask(priority, base::Bind(func, history_backend_.get()));
  }

  template<typename BackendFunc, typename ArgA>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a) {
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    ScheduleTask(priority, base::Bind(func, history_backend_.get(), a));
  }

  template<typename BackendFunc, typename ArgA, typename ArgB>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a,
                         const ArgB& b) {
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    ScheduleTask(priority, base::Bind(func, history_backend_.get(), a, b));
  }

  template<typename BackendFunc, typename ArgA, typename ArgB, typename ArgC>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a,
                         const ArgB& b,
                         const ArgC& c) {
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    ScheduleTask(priority, base::Bind(func, history_backend_.get(), a, b, c));
  }

  template<typename BackendFunc,
           typename ArgA,
           typename ArgB,
           typename ArgC,
           typename ArgD>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a,
                         const ArgB& b,
                         const ArgC& c,
                         const ArgD& d) {
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    ScheduleTask(priority, base::Bind(func, history_backend_.get(),
                                      a, b, c, d));
  }

  template<typename BackendFunc,
           typename ArgA,
           typename ArgB,
           typename ArgC,
           typename ArgD,
           typename ArgE>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a,
                         const ArgB& b,
                         const ArgC& c,
                         const ArgD& d,
                         const ArgE& e) {
    DCHECK(thread_) << "History service being called after cleanup";
    DCHECK(thread_checker_.CalledOnValidThread());
    LoadBackendIfNecessary();
    ScheduleTask(priority, base::Bind(func, history_backend_.get(),
                                      a, b, c, d, e));
  }

  // All vended weak pointers are invalidated in Cleanup().
  base::WeakPtrFactory<HistoryService> weak_ptr_factory_;

  base::ThreadChecker thread_checker_;

  content::NotificationRegistrar registrar_;

  // Some void primitives require some internal processing in the main thread
  // when done. We use this internal consumer for this purpose.
  CancelableRequestConsumer internal_consumer_;

  // The thread used by the history service to run complicated operations.
  // |thread_| is NULL once |Cleanup| is NULL.
  base::Thread* thread_;

  // This class has most of the implementation and runs on the 'thread_'.
  // You MUST communicate with this class ONLY through the thread_'s
  // message_loop().
  //
  // This pointer will be NULL once Cleanup() has been called, meaning no
  // more calls should be made to the history thread.
  scoped_refptr<history::HistoryBackend> history_backend_;

  // A cache of the user-typed URLs kept in memory that is used by the
  // autocomplete system. This will be NULL until the database has been created
  // on the background thread.
  // TODO(mrossetti): Consider changing ownership. See http://crbug.com/138321
  scoped_ptr<history::InMemoryHistoryBackend> in_memory_backend_;

  // Used to propagate local delete directives to sync.
  scoped_ptr<syncer::SyncChangeProcessor> sync_change_processor_;

  // The profile, may be null when testing.
  Profile* profile_;

  // Used for propagating link highlighting data across renderers. May be null
  // in tests.
  scoped_ptr<VisitedLinkMaster> visitedlink_master_;

  // Has the backend finished loading? The backend is loaded once Init has
  // completed.
  bool backend_loaded_;

  // The id of the current backend. This is only valid when history_backend_
  // is not NULL.
  int current_backend_id_;

  // Cached values from Init(), used whenever we need to reload the backend.
  FilePath history_dir_;
  BookmarkService* bookmark_service_;
  bool no_db_;

  // True if needs top site migration.
  bool needs_top_sites_migration_;

  // The index used for quick history lookups.
  // TODO(mrossetti): Move in_memory_url_index out of history_service.
  // See http://crbug.com/138321
  scoped_ptr<history::InMemoryURLIndex> in_memory_url_index_;

  ObserverList<history::VisitDatabaseObserver> visit_database_observers_;

  DISALLOW_COPY_AND_ASSIGN(HistoryService);
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_H_
