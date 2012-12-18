// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The history system runs on a background thread so that potentially slow
// database operations don't delay the browser. This backend processing is
// represented by HistoryBackend. The HistoryService's job is to dispatch to
// that thread.
//
// Main thread                       History thread
// -----------                       --------------
// HistoryService <----------------> HistoryBackend
//                                   -> HistoryDatabase
//                                      -> SQLite connection to History
//                                   -> ArchivedDatabase
//                                      -> SQLite connection to Archived History
//                                   -> TextDatabaseManager
//                                      -> SQLite connection to one month's data
//                                      -> SQLite connection to one month's data
//                                      ...
//                                   -> ThumbnailDatabase
//                                      -> SQLite connection to Thumbnails
//                                         (and favicons)

#include "chrome/browser/history/history.h"

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/sequenced_task_runner.h"
#include "base/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/browser/autocomplete/history_url_provider.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/visit_database.h"
#include "chrome/browser/history/visit_filter.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/browser/visitedlink/visitedlink_master.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/protocol/history_delete_directive_specifics.pb.h"
#include "sync/protocol/proto_value_conversions.h"
#include "sync/protocol/sync.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::Time;
using history::HistoryBackend;

namespace {

static const char* kHistoryThreadName = "Chrome_HistoryThread";

std::string DeleteDirectiveToString(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  scoped_ptr<base::DictionaryValue> value(
      syncer::HistoryDeleteDirectiveSpecificsToValue(delete_directive));
  std::string str;
  base::JSONWriter::Write(value.get(), &str);
  return str;
}

void DerefDownloadDbHandle(
    const HistoryService::DownloadCreateCallback& callback,
    int64* db_handle) {
  callback.Run(*db_handle);
}

void DerefDownloadId(
    const HistoryService::DownloadNextIdCallback& callback,
    int* id) {
  callback.Run(*id);
}

void RunWithFaviconResults(
    const FaviconService::FaviconResultsCallback& callback,
    const HistoryBackend::FaviconResults* results) {
  callback.Run(results->bitmap_results, results->size_map);
}

}  // namespace

// Sends messages from the backend to us on the main thread. This must be a
// separate class from the history service so that it can hold a reference to
// the history service (otherwise we would have to manually AddRef and
// Release when the Backend has a reference to us).
class HistoryService::BackendDelegate : public HistoryBackend::Delegate {
 public:
  BackendDelegate(
      const base::WeakPtr<HistoryService>& history_service,
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner,
      Profile* profile)
      : history_service_(history_service),
        service_task_runner_(service_task_runner),
        profile_(profile) {
  }

  virtual void NotifyProfileError(int backend_id,
                                  sql::InitStatus init_status) OVERRIDE {
    // Send to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::NotifyProfileError, history_service_,
                   backend_id, init_status));
  }

  virtual void SetInMemoryBackend(int backend_id,
      history::InMemoryHistoryBackend* backend) OVERRIDE {
    // Send the backend to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::SetInMemoryBackend, history_service_,
                   backend_id, backend));
  }

  virtual void BroadcastNotifications(
      int type,
      history::HistoryDetails* details) OVERRIDE {
    // Send the notification on the history thread.
    if (content::NotificationService::current()) {
      content::Details<history::HistoryDetails> det(details);
      content::NotificationService::current()->Notify(
          type, content::Source<Profile>(profile_), det);
    }
    // Send the notification to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::BroadcastNotificationsHelper,
                   history_service_, type, base::Owned(details)));
  }

  virtual void DBLoaded(int backend_id) OVERRIDE {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::OnDBLoaded, history_service_,
                   backend_id));
  }

  virtual void StartTopSitesMigration(int backend_id) OVERRIDE {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::StartTopSitesMigration,
                   history_service_, backend_id));
  }

  virtual void NotifyVisitDBObserversOnAddVisit(
      const history::BriefVisitInfo& info) OVERRIDE {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::NotifyVisitDBObserversOnAddVisit,
                   history_service_, info));
  }

 private:
  const base::WeakPtr<HistoryService> history_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
  Profile* const profile_;
};

// The history thread is intentionally not a BrowserThread because the
// sync integration unit tests depend on being able to create more than one
// history thread.
HistoryService::HistoryService()
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      thread_(new base::Thread(kHistoryThreadName)),
      profile_(NULL),
      backend_loaded_(false),
      current_backend_id_(-1),
      bookmark_service_(NULL),
      no_db_(false),
      needs_top_sites_migration_(false) {
}

HistoryService::HistoryService(Profile* profile)
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      thread_(new base::Thread(kHistoryThreadName)),
      profile_(profile),
      backend_loaded_(false),
      current_backend_id_(-1),
      bookmark_service_(NULL),
      no_db_(false),
      needs_top_sites_migration_(false) {
  DCHECK(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_TEMPLATE_URL_REMOVED,
                 content::Source<Profile>(profile_));
}

HistoryService::~HistoryService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
  Cleanup();
}

bool HistoryService::BackendLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // NOTE: We start the backend loading even though it completes asynchronously
  // and thus won't affect the return value of this function.  This is because
  // callers of this assume that if the backend isn't yet loaded it will be
  // soon, so they will either listen for notifications or just retry this call
  // later.  If we've purged the backend, we haven't necessarily restarted it
  // loading by now, so we need to trigger the load in order to maintain that
  // expectation.
  LoadBackendIfNecessary();
  return backend_loaded_;
}

void HistoryService::UnloadBackend() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!history_backend_)
    return;  // Already unloaded.

  // Get rid of the in-memory backend.
  in_memory_backend_.reset();

  // Give the InMemoryURLIndex a chance to shutdown.
  if (in_memory_url_index_.get())
    in_memory_url_index_->ShutDown();

  // The backend's destructor must run on the history thread since it is not
  // threadsafe. So this thread must not be the last thread holding a reference
  // to the backend, or a crash could happen.
  //
  // We have a reference to the history backend. There is also an extra
  // reference held by our delegate installed in the backend, which
  // HistoryBackend::Closing will release. This means if we scheduled a call
  // to HistoryBackend::Closing and *then* released our backend reference, there
  // will be a race between us and the backend's Closing function to see who is
  // the last holder of a reference. If the backend thread's Closing manages to
  // run before we release our backend refptr, the last reference will be held
  // by this thread and the destructor will be called from here.
  //
  // Therefore, we create a closure to run the Closing operation first. This
  // holds a reference to the backend. Then we release our reference, then we
  // schedule the task to run. After the task runs, it will delete its reference
  // from the history thread, ensuring everything works properly.
  //
  // TODO(ajwong): Cleanup HistoryBackend lifetime issues.
  //     See http://crbug.com/99767.
  history_backend_->AddRef();
  base::Closure closing_task =
      base::Bind(&HistoryBackend::Closing, history_backend_.get());
  ScheduleTask(PRIORITY_NORMAL, closing_task);
  closing_task.Reset();
  HistoryBackend* raw_ptr = history_backend_.get();
  history_backend_ = NULL;
  thread_->message_loop()->ReleaseSoon(FROM_HERE, raw_ptr);
}

void HistoryService::Cleanup() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!thread_) {
    // We've already cleaned up.
    return;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Unload the backend.
  UnloadBackend();

  // Delete the thread, which joins with the background thread. We defensively
  // NULL the pointer before deleting it in case somebody tries to use it
  // during shutdown, but this shouldn't happen.
  base::Thread* thread = thread_;
  thread_ = NULL;
  delete thread;
}

void HistoryService::NotifyRenderProcessHostDestruction(const void* host) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::NotifyRenderProcessHostDestruction, host);
}

history::URLDatabase* HistoryService::InMemoryDatabase() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // NOTE: See comments in BackendLoaded() as to why we call
  // LoadBackendIfNecessary() here even though it won't affect the return value
  // for this call.
  LoadBackendIfNecessary();
  if (in_memory_backend_.get())
    return in_memory_backend_->db();
  return NULL;
}

bool HistoryService::GetTypedCountForURL(const GURL& url, int* typed_count) {
  DCHECK(thread_checker_.CalledOnValidThread());
  history::URLRow url_row;
  if (!GetRowForURL(url, &url_row))
    return false;
  *typed_count = url_row.typed_count();
  return true;
}

bool HistoryService::GetLastVisitTimeForURL(const GURL& url,
                                            base::Time* last_visit) {
  DCHECK(thread_checker_.CalledOnValidThread());
  history::URLRow url_row;
  if (!GetRowForURL(url, &url_row))
    return false;
  *last_visit = url_row.last_visit();
  return true;
}

bool HistoryService::GetVisitCountForURL(const GURL& url, int* visit_count) {
  DCHECK(thread_checker_.CalledOnValidThread());
  history::URLRow url_row;
  if (!GetRowForURL(url, &url_row))
    return false;
  *visit_count = url_row.visit_count();
  return true;
}

void HistoryService::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // It's possible that bookmarks haven't loaded and history is waiting for
  // bookmarks to complete loading. In such a situation history can't shutdown
  // (meaning if we invoked history_service_->Cleanup now, we would
  // deadlock). To break the deadlock we tell BookmarkModel it's about to be
  // deleted so that it can release the signal history is waiting on, allowing
  // history to shutdown (history_service_->Cleanup to complete). In such a
  // scenario history sees an incorrect view of bookmarks, but it's better
  // than a deadlock.
  BookmarkModel* bookmark_model = static_cast<BookmarkModel*>(
      BookmarkModelFactory::GetForProfileIfExists(profile_));
  if (bookmark_model)
    bookmark_model->Shutdown();

  Cleanup();
}

void HistoryService::SetSegmentPresentationIndex(int64 segment_id, int index) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_UI,
                    &HistoryBackend::SetSegmentPresentationIndex,
                    segment_id, index);
}

void HistoryService::SetKeywordSearchTermsForURL(const GURL& url,
                                                 TemplateURLID keyword_id,
                                                 const string16& term) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_UI,
                    &HistoryBackend::SetKeywordSearchTermsForURL,
                    url, keyword_id, term);
}

void HistoryService::DeleteAllSearchTermsForKeyword(
    TemplateURLID keyword_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_UI,
                    &HistoryBackend::DeleteAllSearchTermsForKeyword,
                    keyword_id);
}

HistoryService::Handle HistoryService::GetMostRecentKeywordSearchTerms(
    TemplateURLID keyword_id,
    const string16& prefix,
    int max_count,
    CancelableRequestConsumerBase* consumer,
    const GetMostRecentKeywordSearchTermsCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_UI, &HistoryBackend::GetMostRecentKeywordSearchTerms,
                  consumer,
                  new history::GetMostRecentKeywordSearchTermsRequest(callback),
                  keyword_id, prefix, max_count);
}

void HistoryService::URLsNoLongerBookmarked(const std::set<GURL>& urls) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::URLsNoLongerBookmarked,
                    urls);
}

void HistoryService::ScheduleDBTask(HistoryDBTask* task,
                                    CancelableRequestConsumerBase* consumer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  history::HistoryDBTaskRequest* request = new history::HistoryDBTaskRequest(
      base::Bind(&HistoryDBTask::DoneRunOnMainThread, task));
  request->value = task;  // The value is the task to execute.
  Schedule(PRIORITY_UI, &HistoryBackend::ProcessDBTask, consumer, request);
}

HistoryService::Handle HistoryService::QuerySegmentUsageSince(
    CancelableRequestConsumerBase* consumer,
    const Time from_time,
    int max_result_count,
    const SegmentQueryCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_UI, &HistoryBackend::QuerySegmentUsage,
                  consumer, new history::QuerySegmentUsageRequest(callback),
                  from_time, max_result_count);
}

void HistoryService::SetOnBackendDestroyTask(const base::Closure& task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::SetOnBackendDestroyTask,
                    MessageLoop::current(), task);
}

void HistoryService::AddPage(const GURL& url,
                             Time time,
                             const void* id_scope,
                             int32 page_id,
                             const GURL& referrer,
                             const history::RedirectList& redirects,
                             content::PageTransition transition,
                             history::VisitSource visit_source,
                             bool did_replace_entry) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AddPage(
      history::HistoryAddPageArgs(url, time, id_scope, page_id, referrer,
                                  redirects, transition, visit_source,
                                  did_replace_entry));
}

void HistoryService::AddPage(const GURL& url,
                             base::Time time,
                             history::VisitSource visit_source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AddPage(
      history::HistoryAddPageArgs(url, time, NULL, 0, GURL(),
                                  history::RedirectList(),
                                  content::PAGE_TRANSITION_LINK,
                                  visit_source, false));
}

void HistoryService::AddPage(const history::HistoryAddPageArgs& add_page_args) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(thread_) << "History service being called after cleanup";

  // Filter out unwanted URLs. We don't add auto-subframe URLs. They are a
  // large part of history (think iframes for ads) and we never display them in
  // history UI. We will still add manual subframes, which are ones the user
  // has clicked on to get.
  if (!CanAddURL(add_page_args.url))
    return;

  // Add link & all redirects to visited link list.
  VisitedLinkMaster* visited_links;
  if (profile_ &&
      (visited_links = VisitedLinkMaster::FromProfile(profile_))) {
    visited_links->AddURL(add_page_args.url);

    if (!add_page_args.redirects.empty()) {
      // We should not be asked to add a page in the middle of a redirect chain.
      DCHECK_EQ(add_page_args.url,
                add_page_args.redirects[add_page_args.redirects.size() - 1]);

      // We need the !redirects.empty() condition above since size_t is unsigned
      // and will wrap around when we subtract one from a 0 size.
      for (size_t i = 0; i < add_page_args.redirects.size() - 1; i++)
        visited_links->AddURL(add_page_args.redirects[i]);
    }
  }

  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::AddPage, add_page_args);
}

void HistoryService::AddPageNoVisitForBookmark(const GURL& url,
                                               const string16& title) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!CanAddURL(url))
    return;

  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::AddPageNoVisitForBookmark, url, title);
}

void HistoryService::SetPageTitle(const GURL& url,
                                  const string16& title) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::SetPageTitle, url, title);
}

void HistoryService::UpdateWithPageEndTime(const void* host,
                                           int32 page_id,
                                           const GURL& url,
                                           Time end_ts) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::UpdateWithPageEndTime,
                    host, page_id, url, end_ts);
}

void HistoryService::AddPageWithDetails(const GURL& url,
                                        const string16& title,
                                        int visit_count,
                                        int typed_count,
                                        Time last_visit,
                                        bool hidden,
                                        history::VisitSource visit_source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Filter out unwanted URLs.
  if (!CanAddURL(url))
    return;

  // Add to the visited links system.
  VisitedLinkMaster* visited_links;
  if (profile_ &&
      (visited_links = VisitedLinkMaster::FromProfile(profile_))) {
    visited_links->AddURL(url);
  }

  history::URLRow row(url);
  row.set_title(title);
  row.set_visit_count(visit_count);
  row.set_typed_count(typed_count);
  row.set_last_visit(last_visit);
  row.set_hidden(hidden);

  history::URLRows rows;
  rows.push_back(row);

  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::AddPagesWithDetails, rows, visit_source);
}

void HistoryService::AddPagesWithDetails(const history::URLRows& info,
                                         history::VisitSource visit_source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Add to the visited links system.
  VisitedLinkMaster* visited_links;
  if (profile_ &&
      (visited_links = VisitedLinkMaster::FromProfile(profile_))) {
    std::vector<GURL> urls;
    urls.reserve(info.size());
    for (history::URLRows::const_iterator i = info.begin(); i != info.end();
         ++i)
      urls.push_back(i->url());

    visited_links->AddURLs(urls);
  }

  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::AddPagesWithDetails, info, visit_source);
}

void HistoryService::SetPageContents(const GURL& url,
                                     const string16& contents) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!CanAddURL(url))
    return;

  ScheduleAndForget(PRIORITY_LOW, &HistoryBackend::SetPageContents,
                    url, contents);
}

HistoryService::Handle HistoryService::GetPageThumbnail(
    const GURL& page_url,
    CancelableRequestConsumerBase* consumer,
    const ThumbnailDataCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_NORMAL, &HistoryBackend::GetPageThumbnail, consumer,
                  new history::GetPageThumbnailRequest(callback), page_url);
}

CancelableTaskTracker::TaskId HistoryService::GetFavicons(
    const std::vector<GURL>& icon_urls,
    int icon_types,
    int desired_size_in_dip,
    const std::vector<ui::ScaleFactor>& desired_scale_factors,
    const FaviconService::FaviconResultsCallback& callback,
    CancelableTaskTracker* tracker) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LoadBackendIfNecessary();

  HistoryBackend::FaviconResults* results =
      new HistoryBackend::FaviconResults();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy(),
      FROM_HERE,
      base::Bind(&HistoryBackend::GetFavicons,
                 history_backend_.get(), icon_urls, icon_types,
                 desired_size_in_dip, desired_scale_factors, results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

CancelableTaskTracker::TaskId HistoryService::GetFaviconsForURL(
    const GURL& page_url,
    int icon_types,
    int desired_size_in_dip,
    const std::vector<ui::ScaleFactor>& desired_scale_factors,
    const FaviconService::FaviconResultsCallback& callback,
    CancelableTaskTracker* tracker) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LoadBackendIfNecessary();

  HistoryBackend::FaviconResults* results =
      new HistoryBackend::FaviconResults();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy(),
      FROM_HERE,
      base::Bind(&HistoryBackend::GetFaviconsForURL,
                 history_backend_.get(), page_url, icon_types,
                 desired_size_in_dip, desired_scale_factors, results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

CancelableTaskTracker::TaskId HistoryService::GetFaviconForID(
    history::FaviconID favicon_id,
    int desired_size_in_dip,
    ui::ScaleFactor desired_scale_factor,
    const FaviconService::FaviconResultsCallback& callback,
    CancelableTaskTracker* tracker) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LoadBackendIfNecessary();

  HistoryBackend::FaviconResults* results =
      new HistoryBackend::FaviconResults();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy(),
      FROM_HERE,
      base::Bind(&HistoryBackend::GetFaviconForID,
                 history_backend_.get(), favicon_id,
                 desired_size_in_dip, desired_scale_factor, results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

CancelableTaskTracker::TaskId HistoryService::UpdateFaviconMappingsAndFetch(
    const GURL& page_url,
    const std::vector<GURL>& icon_urls,
    int icon_types,
    int desired_size_in_dip,
    const std::vector<ui::ScaleFactor>& desired_scale_factors,
    const FaviconService::FaviconResultsCallback& callback,
    CancelableTaskTracker* tracker) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LoadBackendIfNecessary();

  HistoryBackend::FaviconResults* results =
      new HistoryBackend::FaviconResults();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy(),
      FROM_HERE,
      base::Bind(&HistoryBackend::UpdateFaviconMappingsAndFetch,
                 history_backend_.get(), page_url, icon_urls, icon_types,
                 desired_size_in_dip, desired_scale_factors, results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

void HistoryService::MergeFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    history::IconType icon_type,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    const gfx::Size& pixel_size) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!CanAddURL(page_url))
    return;

  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::MergeFavicon, page_url,
                    icon_url, icon_type, bitmap_data, pixel_size);
}

void HistoryService::SetFavicons(
    const GURL& page_url,
    history::IconType icon_type,
    const std::vector<history::FaviconBitmapData>& favicon_bitmap_data,
    const history::IconURLSizesMap& icon_url_sizes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!CanAddURL(page_url))
    return;

  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::SetFavicons, page_url,
      icon_type, favicon_bitmap_data, icon_url_sizes);
}

void HistoryService::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::SetFaviconsOutOfDateForPage, page_url);
}

void HistoryService::CloneFavicons(const GURL& old_page_url,
                                   const GURL& new_page_url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::CloneFavicons,
                    old_page_url, new_page_url);
}

void HistoryService::SetImportedFavicons(
    const std::vector<history::ImportedFaviconUsage>& favicon_usage) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::SetImportedFavicons, favicon_usage);
}

void HistoryService::IterateURLs(URLEnumerator* enumerator) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::IterateURLs, enumerator);
}

HistoryService::Handle HistoryService::QueryURL(
    const GURL& url,
    bool want_visits,
    CancelableRequestConsumerBase* consumer,
    const QueryURLCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_UI, &HistoryBackend::QueryURL, consumer,
                  new history::QueryURLRequest(callback), url, want_visits);
}

// Downloads -------------------------------------------------------------------

// Handle creation of a download by creating an entry in the history service's
// 'downloads' table.
void HistoryService::CreateDownload(
    const history::DownloadRow& create_info,
    const HistoryService::DownloadCreateCallback& callback) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  LoadBackendIfNecessary();
  int64* db_handle = new int64(
      history::DownloadDatabase::kUninitializedHandle);
  thread_->message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&HistoryBackend::CreateDownload,
                 history_backend_.get(),
                 create_info,
                 db_handle),
      base::Bind(&DerefDownloadDbHandle, callback, base::Owned(db_handle)));
}

void HistoryService::GetNextDownloadId(const DownloadNextIdCallback& callback) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  LoadBackendIfNecessary();
  int* id = new int(history::DownloadDatabase::kUninitializedHandle);
  thread_->message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&HistoryBackend::GetNextDownloadId,
                 history_backend_.get(),
                 id),
      base::Bind(&DerefDownloadId, callback, base::Owned(id)));
}

// Handle queries for a list of all downloads in the history database's
// 'downloads' table.
void HistoryService::QueryDownloads(
    const DownloadQueryCallback& callback) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  LoadBackendIfNecessary();
  std::vector<history::DownloadRow>* rows =
    new std::vector<history::DownloadRow>();
  scoped_ptr<std::vector<history::DownloadRow> > scoped_rows(rows);
  // Beware! The first Bind() does not simply |scoped_rows.get()| because
  // base::Passed(&scoped_rows) nullifies |scoped_rows|, and compilers do not
  // guarantee that the first Bind's arguments are evaluated before the second
  // Bind's arguments.
  thread_->message_loop_proxy()->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&HistoryBackend::QueryDownloads, history_backend_.get(), rows),
      base::Bind(callback, base::Passed(&scoped_rows)));
}

// Changes all IN_PROGRESS in the database entries to CANCELED.
// IN_PROGRESS entries are the corrupted entries, not updated by next function
// because of the crash or some other extremal exit.
void HistoryService::CleanUpInProgressEntries() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::CleanUpInProgressEntries);
}

// Handle updates for a particular download. This is a 'fire and forget'
// operation, so we don't need to be called back.
void HistoryService::UpdateDownload(const history::DownloadRow& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::UpdateDownload, data);
}

void HistoryService::RemoveDownloads(const std::set<int64>& db_handles) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::RemoveDownloads, db_handles);
}

HistoryService::Handle HistoryService::QueryHistory(
    const string16& text_query,
    const history::QueryOptions& options,
    CancelableRequestConsumerBase* consumer,
    const QueryHistoryCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_UI, &HistoryBackend::QueryHistory, consumer,
                  new history::QueryHistoryRequest(callback),
                  text_query, options);
}

HistoryService::Handle HistoryService::QueryRedirectsFrom(
    const GURL& from_url,
    CancelableRequestConsumerBase* consumer,
    const QueryRedirectsCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_UI, &HistoryBackend::QueryRedirectsFrom, consumer,
      new history::QueryRedirectsRequest(callback), from_url);
}

HistoryService::Handle HistoryService::QueryRedirectsTo(
    const GURL& to_url,
    CancelableRequestConsumerBase* consumer,
    const QueryRedirectsCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_NORMAL, &HistoryBackend::QueryRedirectsTo, consumer,
      new history::QueryRedirectsRequest(callback), to_url);
}

HistoryService::Handle HistoryService::GetVisibleVisitCountToHost(
    const GURL& url,
    CancelableRequestConsumerBase* consumer,
    const GetVisibleVisitCountToHostCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_UI, &HistoryBackend::GetVisibleVisitCountToHost,
      consumer, new history::GetVisibleVisitCountToHostRequest(callback), url);
}

HistoryService::Handle HistoryService::QueryTopURLsAndRedirects(
    int result_count,
    CancelableRequestConsumerBase* consumer,
    const QueryTopURLsAndRedirectsCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_NORMAL, &HistoryBackend::QueryTopURLsAndRedirects,
      consumer, new history::QueryTopURLsAndRedirectsRequest(callback),
      result_count);
}

HistoryService::Handle HistoryService::QueryMostVisitedURLs(
    int result_count,
    int days_back,
    CancelableRequestConsumerBase* consumer,
    const QueryMostVisitedURLsCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_NORMAL, &HistoryBackend::QueryMostVisitedURLs,
                  consumer,
                  new history::QueryMostVisitedURLsRequest(callback),
                  result_count, days_back);
}

HistoryService::Handle HistoryService::QueryFilteredURLs(
    int result_count,
    const history::VisitFilter& filter,
    bool extended_info,
    CancelableRequestConsumerBase* consumer,
    const QueryFilteredURLsCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return Schedule(PRIORITY_NORMAL,
                  &HistoryBackend::QueryFilteredURLs,
                  consumer,
                  new history::QueryFilteredURLsRequest(callback),
                  result_count, filter, extended_info);
}

void HistoryService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!thread_)
    return;

  switch (type) {
    case chrome::NOTIFICATION_HISTORY_URLS_DELETED: {
      // Update the visited link system for deleted URLs. We will update the
      // visited link system for added URLs as soon as we get the add
      // notification (we don't have to wait for the backend, which allows us to
      // be faster to update the state).
      //
      // For deleted URLs, we don't typically know what will be deleted since
      // delete notifications are by time. We would also like to be more
      // respectful of privacy and never tell the user something is gone when it
      // isn't. Therefore, we update the delete URLs after the fact.
      if (!profile_)
        return;  // No profile, probably unit testing.
      content::Details<history::URLsDeletedDetails> deleted_details(details);
      VisitedLinkMaster* visited_links =
          VisitedLinkMaster::FromProfile(profile_);
      if (!visited_links)
        return;  // Nobody to update.
      if (deleted_details->all_history)
        visited_links->DeleteAllURLs();
      else  // Delete individual ones.
        visited_links->DeleteURLs(deleted_details->rows);
      break;
    }

    case chrome::NOTIFICATION_TEMPLATE_URL_REMOVED:
      DeleteAllSearchTermsForKeyword(
          *(content::Details<TemplateURLID>(details).ptr()));
      break;

    default:
      NOTREACHED();
  }
}

bool HistoryService::Init(const FilePath& history_dir,
                          BookmarkService* bookmark_service,
                          bool no_db) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!thread_->Start()) {
    Cleanup();
    return false;
  }

  history_dir_ = history_dir;
  bookmark_service_ = bookmark_service;
  no_db_ = no_db;

  if (profile_) {
    std::string languages =
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
    in_memory_url_index_.reset(
        new history::InMemoryURLIndex(profile_, history_dir_, languages));
    in_memory_url_index_->Init();
  }

  // Create the history backend.
  LoadBackendIfNecessary();
  return true;
}

void HistoryService::ScheduleAutocomplete(HistoryURLProvider* provider,
                                          HistoryURLProviderParams* params) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_UI, &HistoryBackend::ScheduleAutocomplete,
                    scoped_refptr<HistoryURLProvider>(provider), params);
}

void HistoryService::ScheduleTask(SchedulePriority priority,
                                  const base::Closure& task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK(thread_);
  CHECK(thread_->message_loop());
  // TODO(brettw): Do prioritization.
  thread_->message_loop()->PostTask(FROM_HERE, task);
}

// static
bool HistoryService::CanAddURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  // TODO: We should allow kChromeUIScheme URLs if they have been explicitly
  // typed.  Right now, however, these are marked as typed even when triggered
  // by a shortcut or menu action.
  if (url.SchemeIs(chrome::kJavaScriptScheme) ||
      url.SchemeIs(chrome::kChromeDevToolsScheme) ||
      url.SchemeIs(chrome::kChromeUIScheme) ||
      url.SchemeIs(chrome::kViewSourceScheme) ||
      url.SchemeIs(chrome::kChromeInternalScheme))
    return false;

  // Allow all about: and chrome: URLs except about:blank, since the user may
  // like to see "chrome://memory/", etc. in their history and autocomplete.
  if (url == GURL(chrome::kAboutBlankURL))
    return false;

  return true;
}

base::WeakPtr<HistoryService> HistoryService::AsWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

void HistoryService::ProcessDeleteDirectiveForTest(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProcessDeleteDirective(delete_directive);
}

syncer::SyncMergeResult HistoryService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  for (syncer::SyncDataList::const_iterator it = initial_sync_data.begin();
       it != initial_sync_data.end(); ++it) {
    DCHECK_EQ(it->GetDataType(), syncer::HISTORY_DELETE_DIRECTIVES);
    ProcessDeleteDirective(it->GetSpecifics().history_delete_directive());
  }
  sync_change_processor_ = sync_processor.Pass();
  return syncer::SyncMergeResult(type);
}

void HistoryService::StopSyncing(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  sync_change_processor_.reset();
}

syncer::SyncDataList HistoryService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  // TODO(akalin): Keep track of existing delete directives.
  return syncer::SyncDataList();
}

syncer::SyncError HistoryService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  for (syncer::SyncChangeList::const_iterator it = change_list.begin();
       it != change_list.end(); ++it) {
    switch (it->change_type()) {
      case syncer::SyncChange::ACTION_ADD:
        ProcessDeleteDirective(
            it->sync_data().GetSpecifics().history_delete_directive());
        break;
      case syncer::SyncChange::ACTION_DELETE:
        // TODO(akalin): Keep track of existing delete directives.
        break;
      default:
        NOTREACHED();
        break;
    }
  }
  return syncer::SyncError();
}

syncer::SyncError HistoryService::ProcessLocalDeleteDirective(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProcessDeleteDirective(delete_directive);
  if (!sync_change_processor_.get()) {
    return syncer::SyncError(
        FROM_HERE,
        "Cannot send local delete directive to sync",
        syncer::HISTORY_DELETE_DIRECTIVES);
  }
  // Generate a random sync tag since history delete directives don't
  // have a 'built-in' ID.  8 bytes should suffice.
  std::string sync_tag = base::RandBytesAsString(8);
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_history_delete_directive()->CopyFrom(
      delete_directive);
  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData(
          sync_tag, sync_tag, entity_specifics);
  syncer::SyncChange change(
      FROM_HERE, syncer::SyncChange::ACTION_ADD, sync_data);
  syncer::SyncChangeList changes(1, change);
  return sync_change_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void HistoryService::SetInMemoryBackend(int backend_id,
    history::InMemoryHistoryBackend* mem_backend) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!history_backend_ || current_backend_id_ != backend_id) {
    DVLOG(1) << "Message from obsolete backend";
    // Cleaning up the memory backend.
    delete mem_backend;
    return;
  }
  DCHECK(!in_memory_backend_.get()) << "Setting mem DB twice";
  in_memory_backend_.reset(mem_backend);

  // The database requires additional initialization once we own it.
  in_memory_backend_->AttachToHistoryService(profile_);
}

void HistoryService::NotifyProfileError(int backend_id,
                                        sql::InitStatus init_status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!history_backend_ || current_backend_id_ != backend_id) {
    DVLOG(1) << "Message from obsolete backend";
    return;
  }
  ShowProfileErrorDialog(
      (init_status == sql::INIT_FAILURE) ?
      IDS_COULDNT_OPEN_PROFILE_ERROR : IDS_PROFILE_TOO_NEW_ERROR);
}

void HistoryService::DeleteURL(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We will update the visited links when we observe the delete notifications.
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::DeleteURL, url);
}

void HistoryService::DeleteURLsForTest(const std::vector<GURL>& urls) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We will update the visited links when we observe the delete
  // notifications.
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::DeleteURLs, urls);
}

void HistoryService::ExpireHistoryBetween(
    const std::set<GURL>& restrict_urls,
    Time begin_time,
    Time end_time,
    const base::Closure& callback,
    CancelableTaskTracker* tracker) {
  DCHECK(thread_);
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(history_backend_.get());
  tracker->PostTaskAndReply(
      thread_->message_loop_proxy(),
      FROM_HERE,
      base::Bind(&HistoryBackend::ExpireHistoryBetween,
                 history_backend_, restrict_urls, begin_time, end_time),
      callback);
}

void HistoryService::BroadcastNotificationsHelper(
    int type,
    history::HistoryDetails* details) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(evanm): this is currently necessitated by generate_profile, which
  // runs without a browser process. generate_profile should really create
  // a browser process, at which point this check can then be nuked.
  if (!g_browser_process)
    return;

  if (!thread_)
    return;

  // The source of all of our notifications is the profile. Note that this
  // pointer is NULL in unit tests.
  content::Source<Profile> source(profile_);

  // The details object just contains the pointer to the object that the
  // backend has allocated for us. The receiver of the notification will cast
  // this to the proper type.
  content::Details<history::HistoryDetails> det(details);

  content::NotificationService::current()->Notify(type, source, det);
}

void HistoryService::LoadBackendIfNecessary() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!thread_ || history_backend_)
    return;  // Failed to init, or already started loading.

  ++current_backend_id_;
  scoped_refptr<HistoryBackend> backend(
      new HistoryBackend(history_dir_,
                         current_backend_id_,
                         new BackendDelegate(
                             weak_ptr_factory_.GetWeakPtr(),
                             base::ThreadTaskRunnerHandle::Get(),
                             profile_),
                         bookmark_service_));
  history_backend_.swap(backend);

  // There may not be a profile when unit testing.
  std::string languages;
  if (profile_) {
    PrefService* prefs = profile_->GetPrefs();
    languages = prefs->GetString(prefs::kAcceptLanguages);
  }
  ScheduleAndForget(PRIORITY_UI, &HistoryBackend::Init, languages, no_db_);
}

void HistoryService::OnDBLoaded(int backend_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!history_backend_ || current_backend_id_ != backend_id) {
    DVLOG(1) << "Message from obsolete backend";
    return;
  }
  backend_loaded_ = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_HISTORY_LOADED,
      content::Source<Profile>(profile_),
      content::Details<HistoryService>(this));
  if (thread_ && profile_) {
    // We don't want to force creation of TopSites.
    history::TopSites* ts = profile_->GetTopSitesWithoutCreating();
    if (ts)
      ts->HistoryLoaded();
  }
}

bool HistoryService::GetRowForURL(const GURL& url, history::URLRow* url_row) {
  DCHECK(thread_checker_.CalledOnValidThread());
  history::URLDatabase* db = InMemoryDatabase();
  return db && (db->GetRowForURL(url, url_row) != 0);
}

void HistoryService::StartTopSitesMigration(int backend_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!history_backend_ || current_backend_id_ != backend_id) {
    DVLOG(1) << "Message from obsolete backend";
    return;
  }
  needs_top_sites_migration_ = true;
  if (thread_ && profile_) {
    // We don't want to force creation of TopSites.
    history::TopSites* ts = profile_->GetTopSitesWithoutCreating();
    if (ts)
      ts->MigrateFromHistory();
  }
}

void HistoryService::OnTopSitesReady() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::MigrateThumbnailsDatabase);
}

void HistoryService::AddVisitDatabaseObserver(
    history::VisitDatabaseObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  visit_database_observers_.AddObserver(observer);
}

void HistoryService::RemoveVisitDatabaseObserver(
    history::VisitDatabaseObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  visit_database_observers_.RemoveObserver(observer);
}

void HistoryService::NotifyVisitDBObserversOnAddVisit(
    const history::BriefVisitInfo& info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  FOR_EACH_OBSERVER(history::VisitDatabaseObserver, visit_database_observers_,
                    OnAddVisit(info));
}

void HistoryService::ProcessDeleteDirective(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DVLOG(1) << "Processing delete directive: "
           << DeleteDirectiveToString(delete_directive);

  base::Closure done_callback =
      base::Bind(&HistoryService::OnDeleteDirectiveProcessed,
                 weak_ptr_factory_.GetWeakPtr(),
                 delete_directive);

  // Execute |done_callback| on return unless we pass it to something
  // else.
  base::ScopedClosureRunner scoped_done_callback(done_callback);

  // Exactly one directive must be filled in.  If not, ignore.
  if (delete_directive.has_global_id_directive() ==
      delete_directive.has_time_range_directive()) {
    return;
  }

  base::Closure backend_task;

  if (delete_directive.has_global_id_directive()) {
    const sync_pb::GlobalIdDirective& global_id_directive =
        delete_directive.global_id_directive();
    std::vector<base::Time> times;
    for (int i = 0; i < global_id_directive.global_id_size(); ++i) {
      int64 global_id = global_id_directive.global_id(i);
      // The global id is just an internal time value.
      base::Time time = base::Time::FromInternalValue(global_id);
      times.push_back(time);
    }

    if (!times.empty()) {
      backend_task =
          base::Bind(&HistoryBackend::ExpireHistoryForTimes,
                     history_backend_, times);
    }
  } else if (delete_directive.has_time_range_directive()) {
    const sync_pb::TimeRangeDirective& time_range_directive =
        delete_directive.time_range_directive();
    // {start,end}_time_usec must both be filled in.  If not, ignore.
    if (!time_range_directive.has_start_time_usec() ||
        !time_range_directive.has_end_time_usec()) {
      return;
    }

    // The directive is for the closed interval [start_time_usec,
    // end_time_usec], but ExpireHistoryBetween() expects the
    // half-open interval [begin_time, end_time), so add 1 to
    // end_time_usec before converting.
    base::Time begin_time =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMicroseconds(
            time_range_directive.start_time_usec());
    base::Time end_time =
        base::Time::UnixEpoch() +
        base::TimeDelta::FromMicroseconds(
            time_range_directive.end_time_usec() + 1);

    backend_task =
        base::Bind(&HistoryBackend::ExpireHistoryBetween,
                   history_backend_, std::set<GURL>(),
                   begin_time, end_time);
  }

  if (!backend_task.is_null()) {
    LoadBackendIfNecessary();
    DCHECK(thread_);
    if (thread_->message_loop_proxy()->PostTaskAndReply(
            FROM_HERE,
            backend_task,
            done_callback)) {
      ignore_result(scoped_done_callback.Release());
    }
  }
}

void HistoryService::OnDeleteDirectiveProcessed(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  DVLOG(1) << "Processed delete directive: "
           << DeleteDirectiveToString(delete_directive);
  // TODO(akalin): Keep track of which delete directives we've already
  // processed.
}
