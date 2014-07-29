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
//                                   -> ThumbnailDatabase
//                                      -> SQLite connection to Thumbnails
//                                         (and favicons)

#include "chrome/browser/history/history_service.h"

#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/visit_database.h"
#include "chrome/browser/history/visit_filter.h"
#include "chrome/browser/history/web_history_service.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/history/core/browser/history_client.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/common/thumbnail_score.h"
#include "components/visitedlink/browser/visitedlink_master.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/notification_service.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "sync/api/sync_error_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"

using base::Time;
using history::HistoryBackend;
using history::KeywordID;

namespace {

static const char* kHistoryThreadName = "Chrome_HistoryThread";

void RunWithFaviconResults(
    const favicon_base::FaviconResultsCallback& callback,
    std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results) {
  callback.Run(*bitmap_results);
}

void RunWithFaviconResult(
    const favicon_base::FaviconRawBitmapCallback& callback,
    favicon_base::FaviconRawBitmapResult* bitmap_result) {
  callback.Run(*bitmap_result);
}

void RunWithQueryURLResult(const HistoryService::QueryURLCallback& callback,
                           const history::QueryURLResult* result) {
  callback.Run(result->success, result->row, result->visits);
}

void RunWithVisibleVisitCountToHostResult(
    const HistoryService::GetVisibleVisitCountToHostCallback& callback,
    const history::VisibleVisitCountToHostResult* result) {
  callback.Run(result->success, result->count, result->first_visit);
}

// Extract history::URLRows into GURLs for VisitedLinkMaster.
class URLIteratorFromURLRows
    : public visitedlink::VisitedLinkMaster::URLIterator {
 public:
  explicit URLIteratorFromURLRows(const history::URLRows& url_rows)
      : itr_(url_rows.begin()),
        end_(url_rows.end()) {
  }

  virtual const GURL& NextURL() OVERRIDE {
    return (itr_++)->url();
  }

  virtual bool HasNextURL() const OVERRIDE {
    return itr_ != end_;
  }

 private:
  history::URLRows::const_iterator itr_;
  history::URLRows::const_iterator end_;

  DISALLOW_COPY_AND_ASSIGN(URLIteratorFromURLRows);
};

// Callback from WebHistoryService::ExpireWebHistory().
void ExpireWebHistoryComplete(bool success) {
  // Ignore the result.
  //
  // TODO(davidben): ExpireLocalAndRemoteHistoryBetween callback should not fire
  // until this completes.
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

  virtual void NotifyProfileError(sql::InitStatus init_status) OVERRIDE {
    // Send to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::NotifyProfileError, history_service_,
                   init_status));
  }

  virtual void SetInMemoryBackend(
      scoped_ptr<history::InMemoryHistoryBackend> backend) OVERRIDE {
    // Send the backend to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::SetInMemoryBackend, history_service_,
                   base::Passed(&backend)));
  }

  virtual void BroadcastNotifications(
      int type,
      scoped_ptr<history::HistoryDetails> details) OVERRIDE {
    // Send the notification on the history thread.
    if (content::NotificationService::current()) {
      content::Details<history::HistoryDetails> det(details.get());
      content::NotificationService::current()->Notify(
          type, content::Source<Profile>(profile_), det);
    }
    // Send the notification to the history service on the main thread.
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::BroadcastNotificationsHelper,
                   history_service_, type, base::Passed(&details)));
  }

  virtual void DBLoaded() OVERRIDE {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&HistoryService::OnDBLoaded, history_service_));
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
    : weak_ptr_factory_(this),
      thread_(new base::Thread(kHistoryThreadName)),
      history_client_(NULL),
      profile_(NULL),
      backend_loaded_(false),
      no_db_(false) {
}

HistoryService::HistoryService(history::HistoryClient* client, Profile* profile)
    : weak_ptr_factory_(this),
      thread_(new base::Thread(kHistoryThreadName)),
      history_client_(client),
      profile_(profile),
      visitedlink_master_(new visitedlink::VisitedLinkMaster(
          profile, this, true)),
      backend_loaded_(false),
      no_db_(false) {
  DCHECK(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                 content::Source<Profile>(profile_));
}

HistoryService::~HistoryService() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Shutdown the backend. This does nothing if Cleanup was already invoked.
  Cleanup();
}

bool HistoryService::BackendLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return backend_loaded_;
}

void HistoryService::Cleanup() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!thread_) {
    // We've already cleaned up.
    return;
  }

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Unload the backend.
  if (history_backend_) {
    // Get rid of the in-memory backend.
    in_memory_backend_.reset();

    // Give the InMemoryURLIndex a chance to shutdown.
    // NOTE: In tests, there may be no index.
    if (in_memory_url_index_)
      in_memory_url_index_->ShutDown();

    // The backend's destructor must run on the history thread since it is not
    // threadsafe. So this thread must not be the last thread holding a
    // reference to the backend, or a crash could happen.
    //
    // We have a reference to the history backend. There is also an extra
    // reference held by our delegate installed in the backend, which
    // HistoryBackend::Closing will release. This means if we scheduled a call
    // to HistoryBackend::Closing and *then* released our backend reference,
    // there will be a race between us and the backend's Closing function to see
    // who is the last holder of a reference. If the backend thread's Closing
    // manages to run before we release our backend refptr, the last reference
    // will be held by this thread and the destructor will be called from here.
    //
    // Therefore, we create a closure to run the Closing operation first. This
    // holds a reference to the backend. Then we release our reference, then we
    // schedule the task to run. After the task runs, it will delete its
    // reference from the history thread, ensuring everything works properly.
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

  // Delete the thread, which joins with the background thread. We defensively
  // NULL the pointer before deleting it in case somebody tries to use it
  // during shutdown, but this shouldn't happen.
  base::Thread* thread = thread_;
  thread_ = NULL;
  delete thread;
}

void HistoryService::ClearCachedDataForContextID(
    history::ContextID context_id) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::ClearCachedDataForContextID, context_id);
}

history::URLDatabase* HistoryService::InMemoryDatabase() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return in_memory_backend_ ? in_memory_backend_->db() : NULL;
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

history::TypedUrlSyncableService* HistoryService::GetTypedUrlSyncableService()
    const {
  return history_backend_->GetTypedUrlSyncableService();
}

void HistoryService::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Cleanup();
}

void HistoryService::SetKeywordSearchTermsForURL(const GURL& url,
                                                 KeywordID keyword_id,
                                                 const base::string16& term) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_UI,
                    &HistoryBackend::SetKeywordSearchTermsForURL,
                    url, keyword_id, term);
}

void HistoryService::DeleteAllSearchTermsForKeyword(KeywordID keyword_id) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  if (in_memory_backend_)
    in_memory_backend_->DeleteAllSearchTermsForKeyword(keyword_id);

  ScheduleAndForget(PRIORITY_UI,
                    &HistoryBackend::DeleteAllSearchTermsForKeyword,
                    keyword_id);
}

void HistoryService::DeleteKeywordSearchTermForURL(const GURL& url) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_UI, &HistoryBackend::DeleteKeywordSearchTermForURL,
                    url);
}

void HistoryService::DeleteMatchingURLsForKeyword(KeywordID keyword_id,
                                                  const base::string16& term) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_UI, &HistoryBackend::DeleteMatchingURLsForKeyword,
                    keyword_id, term);
}

void HistoryService::URLsNoLongerBookmarked(const std::set<GURL>& urls) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::URLsNoLongerBookmarked,
                    urls);
}

void HistoryService::ScheduleDBTask(scoped_ptr<history::HistoryDBTask> task,
                                    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  tracker->NewTrackedTaskId(&is_canceled);
  // Use base::ThreadTaskRunnerHandler::Get() to get a message loop proxy to
  // the current message loop so that we can forward the call to the method
  // HistoryDBTask::DoneRunOnMainThread in the correct thread.
  thread_->message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&HistoryBackend::ProcessDBTask,
                 history_backend_.get(),
                 base::Passed(&task),
                 base::ThreadTaskRunnerHandle::Get(),
                 is_canceled));
}

void HistoryService::FlushForTest(const base::Closure& flushed) {
  thread_->message_loop_proxy()->PostTaskAndReply(
      FROM_HERE, base::Bind(&base::DoNothing), flushed);
}

void HistoryService::SetOnBackendDestroyTask(const base::Closure& task) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::SetOnBackendDestroyTask,
                    base::MessageLoop::current(), task);
}

void HistoryService::AddPage(const GURL& url,
                             Time time,
                             history::ContextID context_id,
                             int32 page_id,
                             const GURL& referrer,
                             const history::RedirectList& redirects,
                             content::PageTransition transition,
                             history::VisitSource visit_source,
                             bool did_replace_entry) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AddPage(
      history::HistoryAddPageArgs(url, time, context_id, page_id, referrer,
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
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());

  // Filter out unwanted URLs. We don't add auto-subframe URLs. They are a
  // large part of history (think iframes for ads) and we never display them in
  // history UI. We will still add manual subframes, which are ones the user
  // has clicked on to get.
  if (!CanAddURL(add_page_args.url))
    return;

  // Add link & all redirects to visited link list.
  if (visitedlink_master_) {
    visitedlink_master_->AddURL(add_page_args.url);

    if (!add_page_args.redirects.empty()) {
      // We should not be asked to add a page in the middle of a redirect chain.
      DCHECK_EQ(add_page_args.url,
                add_page_args.redirects[add_page_args.redirects.size() - 1]);

      // We need the !redirects.empty() condition above since size_t is unsigned
      // and will wrap around when we subtract one from a 0 size.
      for (size_t i = 0; i < add_page_args.redirects.size() - 1; i++)
        visitedlink_master_->AddURL(add_page_args.redirects[i]);
    }
  }

  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::AddPage, add_page_args);
}

void HistoryService::AddPageNoVisitForBookmark(const GURL& url,
                                               const base::string16& title) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!CanAddURL(url))
    return;

  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::AddPageNoVisitForBookmark, url, title);
}

void HistoryService::SetPageTitle(const GURL& url,
                                  const base::string16& title) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::SetPageTitle, url, title);
}

void HistoryService::UpdateWithPageEndTime(history::ContextID context_id,
                                           int32 page_id,
                                           const GURL& url,
                                           Time end_ts) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::UpdateWithPageEndTime,
                    context_id, page_id, url, end_ts);
}

void HistoryService::AddPageWithDetails(const GURL& url,
                                        const base::string16& title,
                                        int visit_count,
                                        int typed_count,
                                        Time last_visit,
                                        bool hidden,
                                        history::VisitSource visit_source) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  // Filter out unwanted URLs.
  if (!CanAddURL(url))
    return;

  // Add to the visited links system.
  if (visitedlink_master_)
    visitedlink_master_->AddURL(url);

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
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  // Add to the visited links system.
  if (visitedlink_master_) {
    std::vector<GURL> urls;
    urls.reserve(info.size());
    for (history::URLRows::const_iterator i = info.begin(); i != info.end();
         ++i)
      urls.push_back(i->url());

    visitedlink_master_->AddURLs(urls);
  }

  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::AddPagesWithDetails, info, visit_source);
}

base::CancelableTaskTracker::TaskId HistoryService::GetFavicons(
    const std::vector<GURL>& icon_urls,
    int icon_types,
    const std::vector<int>& desired_sizes,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::GetFavicons,
                 history_backend_.get(),
                 icon_urls,
                 icon_types,
                 desired_sizes,
                 results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFaviconsForURL(
    const GURL& page_url,
    int icon_types,
    const std::vector<int>& desired_sizes,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::GetFaviconsForURL,
                 history_backend_.get(),
                 page_url,
                 icon_types,
                 desired_sizes,
                 results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetLargestFaviconForURL(
    const GURL& page_url,
    const std::vector<int>& icon_types,
    int minimum_size_in_pixels,
    const favicon_base::FaviconRawBitmapCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  favicon_base::FaviconRawBitmapResult* result =
      new favicon_base::FaviconRawBitmapResult();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::GetLargestFaviconForURL,
                 history_backend_.get(),
                 page_url,
                 icon_types,
                 minimum_size_in_pixels,
                 result),
      base::Bind(&RunWithFaviconResult, callback, base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetFaviconForID(
    favicon_base::FaviconID favicon_id,
    int desired_size,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::GetFaviconForID,
                 history_backend_.get(),
                 favicon_id,
                 desired_size,
                 results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

base::CancelableTaskTracker::TaskId
HistoryService::UpdateFaviconMappingsAndFetch(
    const GURL& page_url,
    const std::vector<GURL>& icon_urls,
    int icon_types,
    const std::vector<int>& desired_sizes,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<favicon_base::FaviconRawBitmapResult>* results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::UpdateFaviconMappingsAndFetch,
                 history_backend_.get(),
                 page_url,
                 icon_urls,
                 icon_types,
                 desired_sizes,
                 results),
      base::Bind(&RunWithFaviconResults, callback, base::Owned(results)));
}

void HistoryService::MergeFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    const gfx::Size& pixel_size) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!CanAddURL(page_url))
    return;

  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::MergeFavicon, page_url,
                    icon_url, icon_type, bitmap_data, pixel_size);
}

void HistoryService::SetFavicons(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    const std::vector<favicon_base::FaviconRawBitmapData>&
        favicon_bitmap_data) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!CanAddURL(page_url))
    return;

  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::SetFavicons, page_url,
      icon_type, favicon_bitmap_data);
}

void HistoryService::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::SetFaviconsOutOfDateForPage, page_url);
}

void HistoryService::CloneFavicons(const GURL& old_page_url,
                                   const GURL& new_page_url) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::CloneFavicons,
                    old_page_url, new_page_url);
}

void HistoryService::SetImportedFavicons(
    const std::vector<ImportedFaviconUsage>& favicon_usage) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::SetImportedFavicons, favicon_usage);
}

base::CancelableTaskTracker::TaskId HistoryService::QueryURL(
    const GURL& url,
    bool want_visits,
    const QueryURLCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  history::QueryURLResult* query_url_result = new history::QueryURLResult();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::QueryURL,
                 history_backend_.get(),
                 url,
                 want_visits,
                 base::Unretained(query_url_result)),
      base::Bind(
          &RunWithQueryURLResult, callback, base::Owned(query_url_result)));
}

// Downloads -------------------------------------------------------------------

// Handle creation of a download by creating an entry in the history service's
// 'downloads' table.
void HistoryService::CreateDownload(
    const history::DownloadRow& create_info,
    const HistoryService::DownloadCreateCallback& callback) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(
      thread_->message_loop_proxy(), FROM_HERE,
      base::Bind(&HistoryBackend::CreateDownload, history_backend_.get(),
                 create_info),
      callback);
}

void HistoryService::GetNextDownloadId(
    const content::DownloadIdCallback& callback) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  PostTaskAndReplyWithResult(
      thread_->message_loop_proxy(), FROM_HERE,
      base::Bind(&HistoryBackend::GetNextDownloadId, history_backend_.get()),
      callback);
}

// Handle queries for a list of all downloads in the history database's
// 'downloads' table.
void HistoryService::QueryDownloads(
    const DownloadQueryCallback& callback) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
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

// Handle updates for a particular download. This is a 'fire and forget'
// operation, so we don't need to be called back.
void HistoryService::UpdateDownload(const history::DownloadRow& data) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::UpdateDownload, data);
}

void HistoryService::RemoveDownloads(const std::set<uint32>& ids) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL,
                    &HistoryBackend::RemoveDownloads, ids);
}

base::CancelableTaskTracker::TaskId HistoryService::QueryHistory(
    const base::string16& text_query,
    const history::QueryOptions& options,
    const QueryHistoryCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  history::QueryResults* query_results = new history::QueryResults();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::QueryHistory,
                 history_backend_.get(),
                 text_query,
                 options,
                 base::Unretained(query_results)),
      base::Bind(callback, base::Owned(query_results)));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryRedirectsFrom(
    const GURL& from_url,
    const QueryRedirectsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  history::RedirectList* result = new history::RedirectList();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::QueryRedirectsFrom,
                 history_backend_.get(),
                 from_url,
                 base::Unretained(result)),
      base::Bind(callback, base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryRedirectsTo(
    const GURL& to_url,
    const QueryRedirectsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  history::RedirectList* result = new history::RedirectList();
  return tracker->PostTaskAndReply(thread_->message_loop_proxy().get(),
                                   FROM_HERE,
                                   base::Bind(&HistoryBackend::QueryRedirectsTo,
                                              history_backend_.get(),
                                              to_url,
                                              base::Unretained(result)),
                                   base::Bind(callback, base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::GetVisibleVisitCountToHost(
    const GURL& url,
    const GetVisibleVisitCountToHostCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  history::VisibleVisitCountToHostResult* result =
      new history::VisibleVisitCountToHostResult();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::GetVisibleVisitCountToHost,
                 history_backend_.get(),
                 url,
                 base::Unretained(result)),
      base::Bind(&RunWithVisibleVisitCountToHostResult,
                 callback,
                 base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryMostVisitedURLs(
    int result_count,
    int days_back,
    const QueryMostVisitedURLsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  history::MostVisitedURLList* result = new history::MostVisitedURLList();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::QueryMostVisitedURLs,
                 history_backend_.get(),
                 result_count,
                 days_back,
                 base::Unretained(result)),
      base::Bind(callback, base::Owned(result)));
}

base::CancelableTaskTracker::TaskId HistoryService::QueryFilteredURLs(
    int result_count,
    const history::VisitFilter& filter,
    bool extended_info,
    const QueryFilteredURLsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  history::FilteredURLList* result = new history::FilteredURLList();
  return tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::QueryFilteredURLs,
                 history_backend_.get(),
                 result_count,
                 filter,
                 extended_info,
                 base::Unretained(result)),
      base::Bind(callback, base::Owned(result)));
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
      if (visitedlink_master_) {
        content::Details<history::URLsDeletedDetails> deleted_details(details);

        if (deleted_details->all_history) {
          visitedlink_master_->DeleteAllURLs();
        } else {
          URLIteratorFromURLRows iterator(deleted_details->rows);
          visitedlink_master_->DeleteURLs(&iterator);
        }
      }
      break;
    }

    default:
      NOTREACHED();
  }
}

void HistoryService::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::IterateURLs, enumerator);
}

bool HistoryService::Init(const base::FilePath& history_dir, bool no_db) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  base::Thread::Options options;
  options.timer_slack = base::TIMER_SLACK_MAXIMUM;
  if (!thread_->StartWithOptions(options)) {
    Cleanup();
    return false;
  }

  history_dir_ = history_dir;
  no_db_ = no_db;

  if (profile_) {
    std::string languages =
        profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
    in_memory_url_index_.reset(new history::InMemoryURLIndex(
        profile_, history_dir_, languages, history_client_));
    in_memory_url_index_->Init();
  }

  // Create the history backend.
  scoped_refptr<HistoryBackend> backend(
      new HistoryBackend(history_dir_,
                         new BackendDelegate(
                             weak_ptr_factory_.GetWeakPtr(),
                             base::ThreadTaskRunnerHandle::Get(),
                             profile_),
                         history_client_));
  history_backend_.swap(backend);

  // There may not be a profile when unit testing.
  std::string languages;
  if (profile_) {
    PrefService* prefs = profile_->GetPrefs();
    languages = prefs->GetString(prefs::kAcceptLanguages);
  }
  ScheduleAndForget(PRIORITY_UI, &HistoryBackend::Init, languages, no_db_);

  if (visitedlink_master_) {
    bool result = visitedlink_master_->Init();
    DCHECK(result);
  }

  return true;
}

void HistoryService::ScheduleAutocomplete(const base::Callback<
    void(history::HistoryBackend*, history::URLDatabase*)>& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ScheduleAndForget(
      PRIORITY_UI, &HistoryBackend::ScheduleAutocomplete, callback);
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
  if (url.SchemeIs(url::kJavaScriptScheme) ||
      url.SchemeIs(content::kChromeDevToolsScheme) ||
      url.SchemeIs(content::kChromeUIScheme) ||
      url.SchemeIs(content::kViewSourceScheme) ||
      url.SchemeIs(chrome::kChromeNativeScheme) ||
      url.SchemeIs(chrome::kChromeSearchScheme) ||
      url.SchemeIs(dom_distiller::kDomDistillerScheme))
    return false;

  // Allow all about: and chrome: URLs except about:blank, since the user may
  // like to see "chrome://memory/", etc. in their history and autocomplete.
  if (url == GURL(url::kAboutBlankURL))
    return false;

  return true;
}

base::WeakPtr<HistoryService> HistoryService::AsWeakPtr() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return weak_ptr_factory_.GetWeakPtr();
}

syncer::SyncMergeResult HistoryService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  delete_directive_handler_.Start(this, initial_sync_data,
                                  sync_processor.Pass());
  return syncer::SyncMergeResult(type);
}

void HistoryService::StopSyncing(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::HISTORY_DELETE_DIRECTIVES);
  delete_directive_handler_.Stop();
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
  delete_directive_handler_.ProcessSyncChanges(this, change_list);
  return syncer::SyncError();
}

syncer::SyncError HistoryService::ProcessLocalDeleteDirective(
    const sync_pb::HistoryDeleteDirectiveSpecifics& delete_directive) {
  DCHECK(thread_checker_.CalledOnValidThread());
  return delete_directive_handler_.ProcessLocalDeleteDirective(
      delete_directive);
}

void HistoryService::SetInMemoryBackend(
    scoped_ptr<history::InMemoryHistoryBackend> mem_backend) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!in_memory_backend_) << "Setting mem DB twice";
  in_memory_backend_.reset(mem_backend.release());

  // The database requires additional initialization once we own it.
  in_memory_backend_->AttachToHistoryService(profile_);
}

void HistoryService::NotifyProfileError(sql::InitStatus init_status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (history_client_)
    history_client_->NotifyProfileError(init_status);
}

void HistoryService::DeleteURL(const GURL& url) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  // We will update the visited links when we observe the delete notifications.
  ScheduleAndForget(PRIORITY_NORMAL, &HistoryBackend::DeleteURL, url);
}

void HistoryService::DeleteURLsForTest(const std::vector<GURL>& urls) {
  DCHECK(thread_) << "History service being called after cleanup";
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
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  tracker->PostTaskAndReply(thread_->message_loop_proxy().get(),
                            FROM_HERE,
                            base::Bind(&HistoryBackend::ExpireHistoryBetween,
                                       history_backend_,
                                       restrict_urls,
                                       begin_time,
                                       end_time),
                            callback);
}

void HistoryService::ExpireHistory(
    const std::vector<history::ExpireHistoryArgs>& expire_list,
    const base::Closure& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(thread_) << "History service being called after cleanup";
  DCHECK(thread_checker_.CalledOnValidThread());
  tracker->PostTaskAndReply(
      thread_->message_loop_proxy().get(),
      FROM_HERE,
      base::Bind(&HistoryBackend::ExpireHistory, history_backend_, expire_list),
      callback);
}

void HistoryService::ExpireLocalAndRemoteHistoryBetween(
    const std::set<GURL>& restrict_urls,
    Time begin_time,
    Time end_time,
    const base::Closure& callback,
    base::CancelableTaskTracker* tracker) {
  // TODO(dubroy): This should be factored out into a separate class that
  // dispatches deletions to the proper places.

  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(profile_);
  if (web_history) {
    // TODO(dubroy): This API does not yet support deletion of specific URLs.
    DCHECK(restrict_urls.empty());

    delete_directive_handler_.CreateDeleteDirectives(
        std::set<int64>(), begin_time, end_time);

    // Attempt online deletion from the history server, but ignore the result.
    // Deletion directives ensure that the results will eventually be deleted.
    //
    // TODO(davidben): |callback| should not run until this operation completes
    // too.
    web_history->ExpireHistoryBetween(
        restrict_urls, begin_time, end_time,
        base::Bind(&ExpireWebHistoryComplete));
  }
  ExpireHistoryBetween(restrict_urls, begin_time, end_time, callback, tracker);
}

void HistoryService::BroadcastNotificationsHelper(
    int type,
    scoped_ptr<history::HistoryDetails> details) {
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
  content::Details<history::HistoryDetails> det(details.get());

  content::NotificationService::current()->Notify(type, source, det);
}

void HistoryService::OnDBLoaded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  backend_loaded_ = true;
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_HISTORY_LOADED,
      content::Source<Profile>(profile_),
      content::Details<HistoryService>(this));
}

bool HistoryService::GetRowForURL(const GURL& url, history::URLRow* url_row) {
  DCHECK(thread_checker_.CalledOnValidThread());
  history::URLDatabase* db = InMemoryDatabase();
  return db && (db->GetRowForURL(url, url_row) != 0);
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
