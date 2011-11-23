// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_H_
#define CHROME_BROWSER_HISTORY_HISTORY_H_
#pragma once

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/task.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/common/ref_counted_util.h"
#include "content/browser/cancelable_request.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/page_transition_types.h"
#include "sql/init_status.h"

class BookmarkService;
struct DownloadPersistentStoreInfo;
class FilePath;
class GURL;
class HistoryURLProvider;
struct HistoryURLProviderParams;
class PageUsageData;
class PageUsageRequest;
class Profile;

namespace base {
class Thread;
class Time;
}

namespace history {
class InMemoryHistoryBackend;
class InMemoryURLIndex;
class HistoryAddPageArgs;
class HistoryBackend;
class HistoryDatabase;
struct HistoryDetails;
class HistoryQueryTest;
class URLDatabase;
}  // namespace history


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

  // Invoked on the main thread once RunOnDBThread has returned false. This is
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
                       public base::RefCountedThreadSafe<HistoryService> {
 public:
  // Miscellaneous commonly-used types.
  typedef std::vector<PageUsageData*> PageUsageDataList;

  // Must call Init after construction.
  explicit HistoryService(Profile* profile);
  // The empty constructor is provided only for testing.
  HistoryService();

  // Initializes the history service, returning true on success. On false, do
  // not call any other functions. The given directory will be used for storing
  // the history files. The BookmarkService is used when deleting URLs to
  // test if a URL is bookmarked; it may be NULL during testing.
  bool Init(const FilePath& history_dir, BookmarkService* bookmark_service) {
    return Init(history_dir, bookmark_service, false);
  }

  // Triggers the backend to load if it hasn't already, and then returns whether
  // it's finished loading.
  bool BackendLoaded();

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

  // Returns the history database if it has been set, otherwise NULL.
  history::URLDatabase* HistoryDatabase();

  // Navigation ----------------------------------------------------------------

  // Adds the given canonical URL to history with the current time as the visit
  // time. Referrer may be the empty string.
  //
  // The supplied render process host is used to scope the given page ID. Page
  // IDs are only unique inside a given render process, so we need that to
  // differentiate them. This pointer should not be dereferenced by the history
  // system. Since render view host pointers may be reused (if one gets deleted
  // and a new one created at the same address), TabContents should notify
  // us when they are being destroyed through NotifyTabContentsDestruction.
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
               const void* id_scope,
               int32 page_id,
               const GURL& referrer,
               content::PageTransition transition,
               const history::RedirectList& redirects,
               history::VisitSource visit_source,
               bool did_replace_entry);

  // For adding pages to history with a specific time. This is for testing
  // purposes. Call the previous one to use the current time.
  void AddPage(const GURL& url,
               base::Time time,
               const void* id_scope,
               int32 page_id,
               const GURL& referrer,
               content::PageTransition transition,
               const history::RedirectList& redirects,
               history::VisitSource visit_source,
               bool did_replace_entry);

  // For adding pages to history where no tracking information can be done.
  void AddPage(const GURL& url, history::VisitSource visit_source) {
    AddPage(url, NULL, 0, GURL(), content::PAGE_TRANSITION_LINK,
            history::RedirectList(), visit_source, false);
  }

  // All AddPage variants end up here.
  void AddPage(const history::HistoryAddPageArgs& add_page_args);

  // Adds an entry for the specified url without creating a visit. This should
  // only be used when bookmarking a page, otherwise the row leaks in the
  // history db (it never gets cleaned).
  void AddPageNoVisitForBookmark(const GURL& url);

  // Sets the title for the given page. The page should be in history. If it
  // is not, this operation is ignored. This call will not update the full
  // text index. The last title set when the page is indexed will be the
  // title in the full text index.
  void SetPageTitle(const GURL& url, const string16& title);

  // Indexing ------------------------------------------------------------------

  // Notifies history of the body text of the given recently-visited URL.
  // If the URL was not visited "recently enough," the history system may
  // discard it.
  void SetPageContents(const GURL& url, const string16& contents);

  // Querying ------------------------------------------------------------------

  // Callback class that a client can implement to iterate over URLs. The
  // callbacks WILL BE CALLED ON THE BACKGROUND THREAD! Your implementation
  // should handle this appropriately.
  class URLEnumerator {
   public:
    virtual ~URLEnumerator() {}

    // Indicates that a URL is available. There will be exactly one call for
    // every URL in history.
    virtual void OnURL(const GURL& url) = 0;

    // Indicates we are done iterating over URLs. Once called, there will be no
    // more callbacks made. This call is guaranteed to occur, even if there are
    // no URLs. If all URLs were iterated, success will be true.
    virtual void OnComplete(bool success) = 0;
  };

  // Enumerate all URLs in history. The given iterator will be owned by the
  // caller, so the caller should ensure it exists until OnComplete is called.
  // You should not generally use this since it will be slow to slurp all URLs
  // in from the database. It is designed for rebuilding the visited link
  // database from history.
  void IterateURLs(URLEnumerator* iterator);

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

  // Request the |result_count| most visited URLs and the chain of
  // redirects leading to each of these URLs. |days_back| is the
  // number of days of history to use. Used by TopSites.
  Handle QueryMostVisitedURLs(int result_count, int days_back,
                              CancelableRequestConsumerBase* consumer,
                              const QueryMostVisitedURLsCallback& callback);

  // Thumbnails ----------------------------------------------------------------

  // Implemented by consumers to get thumbnail data. Called when a request for
  // the thumbnail data is complete. Once this callback is made, the request
  // will be completed and no other calls will be made for that handle.
  //
  // This function will be called even on error conditions or if there is no
  // thumbnail for that page. In these cases, the data pointer will be NULL.
  typedef base::Callback<void(Handle, scoped_refptr<RefCountedBytes>)>
      ThumbnailDataCallback;

  // Requests a page thumbnail. See ThumbnailDataCallback definition above.
  Handle GetPageThumbnail(const GURL& page_url,
                          CancelableRequestConsumerBase* consumer,
                          const ThumbnailDataCallback& callback);

  // Database management operations --------------------------------------------

  // Delete all the information related to a single url.
  void DeleteURL(const GURL& url);

  // Removes all visits in the selected time range (including the start time),
  // updating the URLs accordingly. This deletes the associated data, including
  // the full text index. This function also deletes the associated favicons,
  // if they are no longer referenced. |callback| runs when the expiration is
  // complete. You may use null Time values to do an unbounded delete in
  // either direction.
  // If |restrict_urls| is not empty, only visits to the URLs in this set are
  // removed.
  void ExpireHistoryBetween(const std::set<GURL>& restrict_urls,
                            base::Time begin_time, base::Time end_time,
                            CancelableRequestConsumerBase* consumer,
                            const base::Closure& callback);

  // Downloads -----------------------------------------------------------------

  // Implemented by the caller of 'CreateDownload' below, and is called when the
  // history service has created a new entry for a download in the history db.
  typedef base::Callback<void(int32, int64)> DownloadCreateCallback;

  // Begins a history request to create a new persistent entry for a download.
  // 'info' contains all the download's creation state, and 'callback' runs
  // when the history service request is complete.
  Handle CreateDownload(int32 id,
                        const DownloadPersistentStoreInfo& info,
                        CancelableRequestConsumerBase* consumer,
                        const DownloadCreateCallback& callback);

  // Implemented by the caller of 'GetNextDownloadId' below.
  typedef base::Callback<void(int)> DownloadNextIdCallback;

  // Runs the callback with the next available download id.
  Handle GetNextDownloadId(CancelableRequestConsumerBase* consumer,
                           const DownloadNextIdCallback& callback);

  // Implemented by the caller of 'QueryDownloads' below, and is called when the
  // history service has retrieved a list of all download state. The call
  typedef base::Callback<void(std::vector<DownloadPersistentStoreInfo>*)>
      DownloadQueryCallback;

  // Begins a history request to retrieve the state of all downloads in the
  // history db. 'callback' runs when the history service request is complete,
  // at which point 'info' contains an array of DownloadPersistentStoreInfo, one
  // per download.
  Handle QueryDownloads(CancelableRequestConsumerBase* consumer,
                        const DownloadQueryCallback& callback);

  // Begins a request to clean up entries that has been corrupted (because of
  // the crash, for example).
  void CleanUpInProgressEntries();

  // Called to update the history service about the current state of a download.
  // This is a 'fire and forget' query, so just pass the relevant state info to
  // the database with no need for a callback.
  void UpdateDownload(const DownloadPersistentStoreInfo& data);

  // Called to update the history service about the path of a download.
  // This is a 'fire and forget' query.
  void UpdateDownloadPath(const FilePath& path, int64 db_handle);

  // Permanently remove a download from the history system. This is a 'fire and
  // forget' operation.
  void RemoveDownload(int64 db_handle);

  // Permanently removes all completed download from the history system within
  // the specified range. This function does not delete downloads that are in
  // progress or in the process of being cancelled. This is a 'fire and forget'
  // operation. You can pass is_null times to get unbounded time in either or
  // both directions.
  void RemoveDownloadsBetween(base::Time remove_begin, base::Time remove_end);

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

  // InMemoryURLIndex ----------------------------------------------------------

  // Returns the quick history index.
  history::InMemoryURLIndex* InMemoryIndex();

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
  void SetOnBackendDestroyTask(Task* task);

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
  void AddPagesWithDetails(const std::vector<history::URLRow>& info,
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

 protected:
  virtual ~HistoryService();

  // These are not currently used, hopefully we can do something in the future
  // to ensure that the most important things happen first.
  enum SchedulePriority {
    PRIORITY_UI,      // The highest priority (must respond to UI events).
    PRIORITY_NORMAL,  // Normal stuff like adding a page.
    PRIORITY_LOW,     // Low priority things like indexing or expiration.
  };

 private:
  class BackendDelegate;
  friend class base::RefCountedThreadSafe<HistoryService>;
  friend class BackendDelegate;
  friend class FaviconService;
  friend class history::HistoryBackend;
  friend class history::HistoryQueryTest;
  friend class HistoryOperation;
  friend class HistoryURLProvider;
  friend class HistoryURLProviderTest;
  template<typename Info, typename Callback> friend class DownloadRequest;
  friend class PageUsageRequest;
  friend class RedirectRequest;
  friend class TestingProfile;

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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
  // The |details_deleted| pointer will be sent as the "details" for the
  // notification. The function takes ownership of the pointer and deletes it
  // when the notification is sent (it is coming from another thread, so must
  // be allocated on the heap).
  void BroadcastNotifications(int type,
                              history::HistoryDetails* details_deleted);

  // Initializes the backend.
  void LoadBackendIfNecessary();

  // Notification from the backend that it has finished loading. Sends
  // notification (NOTIFY_HISTORY_LOADED) and sets backend_loaded_ to true.
  void OnDBLoaded(int backend_id);

  // Favicon -------------------------------------------------------------------

  // These favicon methods are exposed to the FaviconService. Instead of calling
  // these methods directly you should call the respective method on the
  // FaviconService.

  // Used by the FaviconService to get a favicon from the history backend.
  void GetFavicon(FaviconService::GetFaviconRequest* request,
                  const GURL& icon_url,
                  history::IconType icon_type);

  // Used by the FaviconService to update the favicon mappings on the history
  // backend.
  void UpdateFaviconMappingAndFetch(FaviconService::GetFaviconRequest* request,
                                    const GURL& page_url,
                                    const GURL& icon_url,
                                    history::IconType icon_type);

  // Used by the FaviconService to get a favicon from the history backend.
  void GetFaviconForURL(FaviconService::GetFaviconRequest* request,
                        const GURL& page_url,
                        int icon_types);

  // Used by the FaviconService to mark the favicon for the page as being out
  // of date.
  void SetFaviconOutOfDateForPage(const GURL& page_url);

  // Used by the FaviconService to clone favicons from one page to another,
  // provided that other page does not already have favicons.
  void CloneFavicon(const GURL& old_page_url, const GURL& new_page_url);

  // Used by the FaviconService for importing many favicons for many pages at
  // once. The pages must exist, any favicon sets for unknown pages will be
  // discarded. Existing favicons will not be overwritten.
  void SetImportedFavicons(
      const std::vector<history::ImportedFaviconUsage>& favicon_usage);

  // Used by the FaviconService to set the favicon for a page on the history
  // backend.
  void SetFavicon(const GURL& page_url,
                  const GURL& icon_url,
                  const std::vector<unsigned char>& image_data,
                  history::IconType icon_type);


  // Sets the in-memory URL database. This is called by the backend once the
  // database is loaded to make it available.
  void SetInMemoryBackend(int backend_id,
                          history::InMemoryHistoryBackend* mem_backend);

  // Called by our BackendDelegate when there is a problem reading the database.
  void NotifyProfileError(int backend_id, sql::InitStatus init_status);

  // Call to schedule a given task for running on the history thread with the
  // specified priority. The task will have ownership taken.
  void ScheduleTask(SchedulePriority priority, const base::Closure& task);

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
    LoadBackendIfNecessary();
    if (consumer)
      AddRequest(request, consumer);
    ScheduleTask(priority,
                 base::Bind(func, history_backend_.get(),
                            scoped_refptr<RequestType>(request), a));
    return request->handle();
  }

  template<typename BackendFunc,
           class RequestType,  // Descendant of CancelableRequstBase.
           typename ArgA,
           typename ArgB>
  Handle Schedule(SchedulePriority priority,
                  BackendFunc func,  // Function to call on the HistoryBackend.
                  CancelableRequestConsumerBase* consumer,
                  RequestType* request,
                  const ArgA& a,
                  const ArgB& b) {
    DCHECK(thread_) << "History service being called after cleanup";
    LoadBackendIfNecessary();
    if (consumer)
      AddRequest(request, consumer);
    ScheduleTask(priority,
                 base::Bind(func, history_backend_.get(),
                            scoped_refptr<RequestType>(request), a, b));
    return request->handle();
  }

  template<typename BackendFunc,
           class RequestType,  // Descendant of CancelableRequstBase.
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
    LoadBackendIfNecessary();
    if (consumer)
      AddRequest(request, consumer);
    ScheduleTask(priority,
                 base::Bind(func, history_backend_.get(),
                            scoped_refptr<RequestType>(request), a, b, c));
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
    LoadBackendIfNecessary();
    ScheduleTask(priority, base::Bind(func, history_backend_.get()));
  }

  template<typename BackendFunc, typename ArgA>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a) {
    DCHECK(thread_) << "History service being called after cleanup";
    LoadBackendIfNecessary();
    ScheduleTask(priority, base::Bind(func, history_backend_.get(), a));
  }

  template<typename BackendFunc, typename ArgA, typename ArgB>
  void ScheduleAndForget(SchedulePriority priority,
                         BackendFunc func,  // Function to call on backend.
                         const ArgA& a,
                         const ArgB& b) {
    DCHECK(thread_) << "History service being called after cleanup";
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
    LoadBackendIfNecessary();
    ScheduleTask(priority, base::Bind(func, history_backend_.get(),
                                      a, b, c, d));
  }

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
  scoped_ptr<history::InMemoryHistoryBackend> in_memory_backend_;

  // The index used for quick history lookups.
  scoped_ptr<history::InMemoryURLIndex> in_memory_url_index_;

  // The profile, may be null when testing.
  Profile* profile_;

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

  DISALLOW_COPY_AND_ASSIGN(HistoryService);
};

#endif  // CHROME_BROWSER_HISTORY_HISTORY_H_
