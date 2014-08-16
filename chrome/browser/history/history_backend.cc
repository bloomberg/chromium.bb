// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_backend.h"

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_enumerator.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_changed_details.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/in_memory_history_backend.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/history/typed_url_syncable_service.h"
#include "chrome/browser/history/typed_url_syncable_service.h"
#include "chrome/browser/history/visit_filter.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/importer/imported_favicon_usage.h"
#include "chrome/common/url_constants.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "components/history/core/browser/history_client.h"
#include "components/history/core/browser/keyword_search_term.h"
#include "components/history/core/browser/page_usage_data.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "sql/error_delegate_util.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/history/android/android_provider_backend.h"
#endif

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

/* The HistoryBackend consists of two components:

    HistoryDatabase (stores past 3 months of history)
      URLDatabase (stores a list of URLs)
      DownloadDatabase (stores a list of downloads)
      VisitDatabase (stores a list of visits for the URLs)
      VisitSegmentDatabase (stores groups of URLs for the most visited view).

    ExpireHistoryBackend (manages deleting things older than 3 months)
*/

namespace history {

namespace {
void RunUnlessCanceled(
    const base::Closure& closure,
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled) {
  if (!is_canceled.Run())
    closure.Run();
}
}  // namespace

#if defined(OS_ANDROID)
// How long we keep segment data for in days. Currently 3 months.
// This value needs to be greater or equal to
// MostVisitedModel::kMostVisitedScope but we don't want to introduce a direct
// dependency between MostVisitedModel and the history backend.
const int kSegmentDataRetention = 90;
#endif

// How long we'll wait to do a commit, so that things are batched together.
const int kCommitIntervalSeconds = 10;

// The amount of time before we re-fetch the favicon.
const int kFaviconRefetchDays = 7;

// The maximum number of items we'll allow in the redirect list before
// deleting some.
const int kMaxRedirectCount = 32;

// The number of days old a history entry can be before it is considered "old"
// and is deleted.
const int kExpireDaysThreshold = 90;

#if defined(OS_ANDROID)
// The maximum number of top sites to track when recording top page visit stats.
const size_t kPageVisitStatsMaxTopSites = 50;
#endif

// Converts from PageUsageData to MostVisitedURL. |redirects| is a
// list of redirects for this URL. Empty list means no redirects.
MostVisitedURL MakeMostVisitedURL(const PageUsageData& page_data,
                                  const RedirectList& redirects) {
  MostVisitedURL mv;
  mv.url = page_data.GetURL();
  mv.title = page_data.GetTitle();
  if (redirects.empty()) {
    // Redirects must contain at least the target url.
    mv.redirects.push_back(mv.url);
  } else {
    mv.redirects = redirects;
    if (mv.redirects[mv.redirects.size() - 1] != mv.url) {
      // The last url must be the target url.
      mv.redirects.push_back(mv.url);
    }
  }
  return mv;
}

// This task is run on a timer so that commits happen at regular intervals
// so they are batched together. The important thing about this class is that
// it supports canceling of the task so the reference to the backend will be
// freed. The problem is that when history is shutting down, there is likely
// to be one of these commits still pending and holding a reference.
//
// The backend can call Cancel to have this task release the reference. The
// task will still run (if we ever get to processing the event before
// shutdown), but it will not do anything.
//
// Note that this is a refcounted object and is not a task in itself. It should
// be assigned to a RunnableMethod.
//
// TODO(brettw): bug 1165182: This should be replaced with a
// base::WeakPtrFactory which will handle everything automatically (like we do
// in ExpireHistoryBackend).
class CommitLaterTask : public base::RefCounted<CommitLaterTask> {
 public:
  explicit CommitLaterTask(HistoryBackend* history_backend)
      : history_backend_(history_backend) {
  }

  // The backend will call this function if it is being destroyed so that we
  // release our reference.
  void Cancel() {
    history_backend_ = NULL;
  }

  void RunCommit() {
    if (history_backend_.get())
      history_backend_->Commit();
  }

 private:
  friend class base::RefCounted<CommitLaterTask>;

  ~CommitLaterTask() {}

  scoped_refptr<HistoryBackend> history_backend_;
};


QueuedHistoryDBTask::QueuedHistoryDBTask(
    scoped_ptr<HistoryDBTask> task,
    scoped_refptr<base::SingleThreadTaskRunner> origin_loop,
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled)
    : task_(task.Pass()), origin_loop_(origin_loop), is_canceled_(is_canceled) {
  DCHECK(task_);
  DCHECK(origin_loop_);
  DCHECK(!is_canceled_.is_null());
}

QueuedHistoryDBTask::~QueuedHistoryDBTask() {
  // Ensure that |task_| is destroyed on its origin thread.
  origin_loop_->PostTask(
      FROM_HERE,
      base::Bind(&base::DeletePointer<HistoryDBTask>,
                 base::Unretained(task_.release())));
}

bool QueuedHistoryDBTask::is_canceled() {
  return is_canceled_.Run();
}

bool QueuedHistoryDBTask::Run(HistoryBackend* backend,
                                        HistoryDatabase* db) {
  return task_->RunOnDBThread(backend, db);
}

void QueuedHistoryDBTask::DoneRun() {
  origin_loop_->PostTask(
      FROM_HERE,
      base::Bind(&RunUnlessCanceled,
                 base::Bind(&HistoryDBTask::DoneRunOnMainThread,
                            base::Unretained(task_.get())),
                 is_canceled_));
}

// HistoryBackend --------------------------------------------------------------

HistoryBackend::HistoryBackend(const base::FilePath& history_dir,
                               Delegate* delegate,
                               HistoryClient* history_client)
    : delegate_(delegate),
      history_dir_(history_dir),
      scheduled_kill_db_(false),
      expirer_(this, history_client),
      recent_redirects_(kMaxRedirectCount),
      backend_destroy_message_loop_(NULL),
      segment_queried_(false),
      history_client_(history_client) {
}

HistoryBackend::~HistoryBackend() {
  DCHECK(!scheduled_commit_.get()) << "Deleting without cleanup";
  STLDeleteContainerPointers(queued_history_db_tasks_.begin(),
                             queued_history_db_tasks_.end());
  queued_history_db_tasks_.clear();

#if defined(OS_ANDROID)
  // Release AndroidProviderBackend before other objects.
  android_provider_backend_.reset();
#endif

  // First close the databases before optionally running the "destroy" task.
  CloseAllDatabases();

  if (!backend_destroy_task_.is_null()) {
    // Notify an interested party (typically a unit test) that we're done.
    DCHECK(backend_destroy_message_loop_);
    backend_destroy_message_loop_->PostTask(FROM_HERE, backend_destroy_task_);
  }

#if defined(OS_ANDROID)
  sql::Connection::Delete(GetAndroidCacheFileName());
#endif
}

void HistoryBackend::Init(const std::string& languages, bool force_fail) {
  if (!force_fail)
    InitImpl(languages);
  delegate_->DBLoaded();
  typed_url_syncable_service_.reset(new TypedUrlSyncableService(this));
  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::Bind(&HistoryBackend::OnMemoryPressure, base::Unretained(this))));
#if defined(OS_ANDROID)
  PopulateMostVisitedURLMap();
#endif
}

void HistoryBackend::SetOnBackendDestroyTask(base::MessageLoop* message_loop,
                                             const base::Closure& task) {
  if (!backend_destroy_task_.is_null())
    DLOG(WARNING) << "Setting more than one destroy task, overriding";
  backend_destroy_message_loop_ = message_loop;
  backend_destroy_task_ = task;
}

void HistoryBackend::Closing() {
  // Any scheduled commit will have a reference to us, we must make it
  // release that reference before we can be destroyed.
  CancelScheduledCommit();

  // Release our reference to the delegate, this reference will be keeping the
  // history service alive.
  delegate_.reset();
}

void HistoryBackend::ClearCachedDataForContextID(ContextID context_id) {
  tracker_.ClearCachedDataForContextID(context_id);
}

base::FilePath HistoryBackend::GetThumbnailFileName() const {
  return history_dir_.Append(chrome::kThumbnailsFilename);
}

base::FilePath HistoryBackend::GetFaviconsFileName() const {
  return history_dir_.Append(chrome::kFaviconsFilename);
}

base::FilePath HistoryBackend::GetArchivedFileName() const {
  return history_dir_.Append(chrome::kArchivedHistoryFilename);
}

#if defined(OS_ANDROID)
base::FilePath HistoryBackend::GetAndroidCacheFileName() const {
  return history_dir_.Append(chrome::kAndroidCacheFilename);
}
#endif

SegmentID HistoryBackend::GetLastSegmentID(VisitID from_visit) {
  // Set is used to detect referrer loops.  Should not happen, but can
  // if the database is corrupt.
  std::set<VisitID> visit_set;
  VisitID visit_id = from_visit;
  while (visit_id) {
    VisitRow row;
    if (!db_->GetRowForVisit(visit_id, &row))
      return 0;
    if (row.segment_id)
      return row.segment_id;  // Found a visit in this change with a segment.

    // Check the referrer of this visit, if any.
    visit_id = row.referring_visit;

    if (visit_set.find(visit_id) != visit_set.end()) {
      NOTREACHED() << "Loop in referer chain, giving up";
      break;
    }
    visit_set.insert(visit_id);
  }
  return 0;
}

SegmentID HistoryBackend::UpdateSegments(
    const GURL& url,
    VisitID from_visit,
    VisitID visit_id,
    content::PageTransition transition_type,
    const Time ts) {
  if (!db_)
    return 0;

  // We only consider main frames.
  if (!content::PageTransitionIsMainFrame(transition_type))
    return 0;

  SegmentID segment_id = 0;
  content::PageTransition t =
      content::PageTransitionStripQualifier(transition_type);

  // Are we at the beginning of a new segment?
  // Note that navigating to an existing entry (with back/forward) reuses the
  // same transition type.  We are not adding it as a new segment in that case
  // because if this was the target of a redirect, we might end up with
  // 2 entries for the same final URL. Ex: User types google.net, gets
  // redirected to google.com. A segment is created for google.net. On
  // google.com users navigates through a link, then press back. That last
  // navigation is for the entry google.com transition typed. We end up adding
  // a segment for that one as well. So we end up with google.net and google.com
  // in the segment table, showing as 2 entries in the NTP.
  // Note also that we should still be updating the visit count for that segment
  // which we are not doing now. It should be addressed when
  // http://crbug.com/96860 is fixed.
  if ((t == content::PAGE_TRANSITION_TYPED ||
       t == content::PAGE_TRANSITION_AUTO_BOOKMARK) &&
      (transition_type & content::PAGE_TRANSITION_FORWARD_BACK) == 0) {
    // If so, create or get the segment.
    std::string segment_name = db_->ComputeSegmentName(url);
    URLID url_id = db_->GetRowForURL(url, NULL);
    if (!url_id)
      return 0;

    segment_id = db_->GetSegmentNamed(segment_name);
    if (!segment_id) {
      segment_id = db_->CreateSegment(url_id, segment_name);
      if (!segment_id) {
        NOTREACHED();
        return 0;
      }
    } else {
      // Note: if we update an existing segment, we update the url used to
      // represent that segment in order to minimize stale most visited
      // images.
      db_->UpdateSegmentRepresentationURL(segment_id, url_id);
    }
  } else {
    // Note: it is possible there is no segment ID set for this visit chain.
    // This can happen if the initial navigation wasn't AUTO_BOOKMARK or
    // TYPED. (For example GENERATED). In this case this visit doesn't count
    // toward any segment.
    segment_id = GetLastSegmentID(from_visit);
    if (!segment_id)
      return 0;
  }

  // Set the segment in the visit.
  if (!db_->SetSegmentID(visit_id, segment_id)) {
    NOTREACHED();
    return 0;
  }

  // Finally, increase the counter for that segment / day.
  if (!db_->IncreaseSegmentVisitCount(segment_id, ts, 1)) {
    NOTREACHED();
    return 0;
  }
  return segment_id;
}

void HistoryBackend::UpdateWithPageEndTime(ContextID context_id,
                                           int32 page_id,
                                           const GURL& url,
                                           Time end_ts) {
  // Will be filled with the URL ID and the visit ID of the last addition.
  VisitID visit_id = tracker_.GetLastVisit(context_id, page_id, url);
  UpdateVisitDuration(visit_id, end_ts);
}

void HistoryBackend::UpdateVisitDuration(VisitID visit_id, const Time end_ts) {
  if (!db_)
    return;

  // Get the starting visit_time for visit_id.
  VisitRow visit_row;
  if (db_->GetRowForVisit(visit_id, &visit_row)) {
    // We should never have a negative duration time even when time is skewed.
    visit_row.visit_duration = end_ts > visit_row.visit_time ?
        end_ts - visit_row.visit_time : TimeDelta::FromMicroseconds(0);
    db_->UpdateVisitRow(visit_row);
  }
}

void HistoryBackend::AddPage(const HistoryAddPageArgs& request) {
  if (!db_)
    return;

  // Will be filled with the URL ID and the visit ID of the last addition.
  std::pair<URLID, VisitID> last_ids(0, tracker_.GetLastVisit(
      request.context_id, request.page_id, request.referrer));

  VisitID from_visit_id = last_ids.second;

  // If a redirect chain is given, we expect the last item in that chain to be
  // the final URL.
  DCHECK(request.redirects.empty() ||
         request.redirects.back() == request.url);

  // If the user is adding older history, we need to make sure our times
  // are correct.
  if (request.time < first_recorded_time_)
    first_recorded_time_ = request.time;

  content::PageTransition request_transition = request.transition;
  content::PageTransition stripped_transition =
    content::PageTransitionStripQualifier(request_transition);
  bool is_keyword_generated =
      (stripped_transition == content::PAGE_TRANSITION_KEYWORD_GENERATED);

  // If the user is navigating to a not-previously-typed intranet hostname,
  // change the transition to TYPED so that the omnibox will learn that this is
  // a known host.
  bool has_redirects = request.redirects.size() > 1;
  if (content::PageTransitionIsMainFrame(request_transition) &&
      (stripped_transition != content::PAGE_TRANSITION_TYPED) &&
      !is_keyword_generated) {
    const GURL& origin_url(has_redirects ?
        request.redirects[0] : request.url);
    if (origin_url.SchemeIs(url::kHttpScheme) ||
        origin_url.SchemeIs(url::kHttpsScheme) ||
        origin_url.SchemeIs(url::kFtpScheme)) {
      std::string host(origin_url.host());
      size_t registry_length =
          net::registry_controlled_domains::GetRegistryLength(
              host,
              net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
              net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
      if (registry_length == 0 && !db_->IsTypedHost(host)) {
        stripped_transition = content::PAGE_TRANSITION_TYPED;
        request_transition =
            content::PageTransitionFromInt(
                stripped_transition |
                content::PageTransitionGetQualifier(request_transition));
      }
    }
  }

  if (!has_redirects) {
    // The single entry is both a chain start and end.
    content::PageTransition t = content::PageTransitionFromInt(
        request_transition |
        content::PAGE_TRANSITION_CHAIN_START |
        content::PAGE_TRANSITION_CHAIN_END);

    // No redirect case (one element means just the page itself).
    last_ids = AddPageVisit(request.url, request.time,
                            last_ids.second, t, request.visit_source);

    // Update the segment for this visit. KEYWORD_GENERATED visits should not
    // result in changing most visited, so we don't update segments (most
    // visited db).
    if (!is_keyword_generated) {
      UpdateSegments(request.url, from_visit_id, last_ids.second, t,
                     request.time);

      // Update the referrer's duration.
      UpdateVisitDuration(from_visit_id, request.time);
    }
  } else {
    // Redirect case. Add the redirect chain.

    content::PageTransition redirect_info =
        content::PAGE_TRANSITION_CHAIN_START;

    RedirectList redirects = request.redirects;
    if (redirects[0].SchemeIs(url::kAboutScheme)) {
      // When the redirect source + referrer is "about" we skip it. This
      // happens when a page opens a new frame/window to about:blank and then
      // script sets the URL to somewhere else (used to hide the referrer). It
      // would be nice to keep all these redirects properly but we don't ever
      // see the initial about:blank load, so we don't know where the
      // subsequent client redirect came from.
      //
      // In this case, we just don't bother hooking up the source of the
      // redirects, so we remove it.
      redirects.erase(redirects.begin());
    } else if (request_transition & content::PAGE_TRANSITION_CLIENT_REDIRECT) {
      redirect_info = content::PAGE_TRANSITION_CLIENT_REDIRECT;
      // The first entry in the redirect chain initiated a client redirect.
      // We don't add this to the database since the referrer is already
      // there, so we skip over it but change the transition type of the first
      // transition to client redirect.
      //
      // The referrer is invalid when restoring a session that features an
      // https tab that redirects to a different host or to http. In this
      // case we don't need to reconnect the new redirect with the existing
      // chain.
      if (request.referrer.is_valid()) {
        DCHECK(request.referrer == redirects[0]);
        redirects.erase(redirects.begin());

        // If the navigation entry for this visit has replaced that for the
        // first visit, remove the CHAIN_END marker from the first visit. This
        // can be called a lot, for example, the page cycler, and most of the
        // time we won't have changed anything.
        VisitRow visit_row;
        if (request.did_replace_entry &&
            db_->GetRowForVisit(last_ids.second, &visit_row) &&
            visit_row.transition & content::PAGE_TRANSITION_CHAIN_END) {
          visit_row.transition = content::PageTransitionFromInt(
              visit_row.transition & ~content::PAGE_TRANSITION_CHAIN_END);
          db_->UpdateVisitRow(visit_row);
        }
      }
    }

    for (size_t redirect_index = 0; redirect_index < redirects.size();
         redirect_index++) {
      content::PageTransition t =
          content::PageTransitionFromInt(stripped_transition | redirect_info);

      // If this is the last transition, add a CHAIN_END marker
      if (redirect_index == (redirects.size() - 1)) {
        t = content::PageTransitionFromInt(
            t | content::PAGE_TRANSITION_CHAIN_END);
      }

      // Record all redirect visits with the same timestamp. We don't display
      // them anyway, and if we ever decide to, we can reconstruct their order
      // from the redirect chain.
      last_ids = AddPageVisit(redirects[redirect_index],
                              request.time, last_ids.second,
                              t, request.visit_source);
      if (t & content::PAGE_TRANSITION_CHAIN_START) {
        // Update the segment for this visit.
        UpdateSegments(redirects[redirect_index],
                       from_visit_id, last_ids.second, t, request.time);

        // Update the visit_details for this visit.
        UpdateVisitDuration(from_visit_id, request.time);
      }

      // Subsequent transitions in the redirect list must all be server
      // redirects.
      redirect_info = content::PAGE_TRANSITION_SERVER_REDIRECT;
    }

    // Last, save this redirect chain for later so we can set titles & favicons
    // on the redirected pages properly.
    recent_redirects_.Put(request.url, redirects);
  }

  // TODO(brettw) bug 1140015: Add an "add page" notification so the history
  // views can keep in sync.

  // Add the last visit to the tracker so we can get outgoing transitions.
  // TODO(evanm): Due to http://b/1194536 we lose the referrers of a subframe
  // navigation anyway, so last_visit_id is always zero for them.  But adding
  // them here confuses main frame history, so we skip them for now.
  if (stripped_transition != content::PAGE_TRANSITION_AUTO_SUBFRAME &&
      stripped_transition != content::PAGE_TRANSITION_MANUAL_SUBFRAME &&
      !is_keyword_generated) {
    tracker_.AddVisit(request.context_id, request.page_id, request.url,
                      last_ids.second);
  }

  ScheduleCommit();
}

void HistoryBackend::InitImpl(const std::string& languages) {
  DCHECK(!db_) << "Initializing HistoryBackend twice";
  // In the rare case where the db fails to initialize a dialog may get shown
  // the blocks the caller, yet allows other messages through. For this reason
  // we only set db_ to the created database if creation is successful. That
  // way other methods won't do anything as db_ is still NULL.

  TimeTicks beginning_time = TimeTicks::Now();

  // Compute the file names.
  base::FilePath history_name = history_dir_.Append(chrome::kHistoryFilename);
  base::FilePath thumbnail_name = GetFaviconsFileName();
  base::FilePath archived_name = GetArchivedFileName();

  // Delete the old index database files which are no longer used.
  DeleteFTSIndexDatabases();

  // History database.
  db_.reset(new HistoryDatabase());

  // Unretained to avoid a ref loop with db_.
  db_->set_error_callback(
      base::Bind(&HistoryBackend::DatabaseErrorCallback,
                 base::Unretained(this)));

  sql::InitStatus status = db_->Init(history_name);
  switch (status) {
    case sql::INIT_OK:
      break;
    case sql::INIT_FAILURE: {
      // A NULL db_ will cause all calls on this object to notice this error
      // and to not continue. If the error callback scheduled killing the
      // database, the task it posted has not executed yet. Try killing the
      // database now before we close it.
      bool kill_db = scheduled_kill_db_;
      if (kill_db)
        KillHistoryDatabase();
      UMA_HISTOGRAM_BOOLEAN("History.AttemptedToFixProfileError", kill_db);
      delegate_->NotifyProfileError(status);
      db_.reset();
      return;
    }
    default:
      NOTREACHED();
  }

  // Fill the in-memory database and send it back to the history service on the
  // main thread.
  {
    scoped_ptr<InMemoryHistoryBackend> mem_backend(new InMemoryHistoryBackend);
    if (mem_backend->Init(history_name))
      delegate_->SetInMemoryBackend(mem_backend.Pass());
  }
  db_->BeginExclusiveMode();  // Must be after the mem backend read the data.

  // Thumbnail database.
  // TODO(shess): "thumbnail database" these days only stores
  // favicons.  Thumbnails are stored in "top sites".  Consider
  // renaming "thumbnail" references to "favicons" or something of the
  // sort.
  thumbnail_db_.reset(new ThumbnailDatabase(history_client_));
  if (thumbnail_db_->Init(thumbnail_name) != sql::INIT_OK) {
    // Unlike the main database, we don't error out when the database is too
    // new because this error is much less severe. Generally, this shouldn't
    // happen since the thumbnail and main database versions should be in sync.
    // We'll just continue without thumbnails & favicons in this case or any
    // other error.
    LOG(WARNING) << "Could not initialize the thumbnail database.";
    thumbnail_db_.reset();
  }

  // Nuke any files corresponding to the legacy Archived History Database, which
  // previously retained expired (> 3 months old) history entries, but, in the
  // end, was not used for much, and consequently has been removed as of M37.
  // TODO(engedy): Remove this code after the end of 2014.
  sql::Connection::Delete(archived_name);

  // Generate the history and thumbnail database metrics only after performing
  // any migration work.
  if (base::RandInt(1, 100) == 50) {
    // Only do this computation sometimes since it can be expensive.
    db_->ComputeDatabaseMetrics(history_name);
    if (thumbnail_db_)
      thumbnail_db_->ComputeDatabaseMetrics();
  }

  expirer_.SetDatabases(db_.get(), thumbnail_db_.get());

  // Open the long-running transaction.
  db_->BeginTransaction();
  if (thumbnail_db_)
    thumbnail_db_->BeginTransaction();

  // Get the first item in our database.
  db_->GetStartDate(&first_recorded_time_);

  // Start expiring old stuff.
  expirer_.StartExpiringOldStuff(TimeDelta::FromDays(kExpireDaysThreshold));

#if defined(OS_ANDROID)
  if (thumbnail_db_) {
    android_provider_backend_.reset(
        new AndroidProviderBackend(GetAndroidCacheFileName(),
                                   db_.get(),
                                   thumbnail_db_.get(),
                                   history_client_,
                                   delegate_.get()));
  }
#endif

  HISTOGRAM_TIMES("History.InitTime",
                  TimeTicks::Now() - beginning_time);
}

void HistoryBackend::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  bool trim_aggressively = memory_pressure_level ==
      base::MemoryPressureListener::MEMORY_PRESSURE_CRITICAL;
  if (db_)
    db_->TrimMemory(trim_aggressively);
  if (thumbnail_db_)
    thumbnail_db_->TrimMemory(trim_aggressively);
}

void HistoryBackend::CloseAllDatabases() {
  if (db_) {
    // Commit the long-running transaction.
    db_->CommitTransaction();
    db_.reset();
    // Forget the first recorded time since the database is closed.
    first_recorded_time_ = base::Time();
  }
  if (thumbnail_db_) {
    thumbnail_db_->CommitTransaction();
    thumbnail_db_.reset();
  }
}

std::pair<URLID, VisitID> HistoryBackend::AddPageVisit(
    const GURL& url,
    Time time,
    VisitID referring_visit,
    content::PageTransition transition,
    VisitSource visit_source) {
  // Top-level frame navigations are visible, everything else is hidden
  bool new_hidden = !content::PageTransitionIsMainFrame(transition);

  // NOTE: This code must stay in sync with
  // ExpireHistoryBackend::ExpireURLsForVisits().
  // TODO(pkasting): http://b/1148304 We shouldn't be marking so many URLs as
  // typed, which would eliminate the need for this code.
  int typed_increment = 0;
  content::PageTransition transition_type =
      content::PageTransitionStripQualifier(transition);
  if ((transition_type == content::PAGE_TRANSITION_TYPED &&
      !content::PageTransitionIsRedirect(transition)) ||
      transition_type == content::PAGE_TRANSITION_KEYWORD_GENERATED)
    typed_increment = 1;

#if defined(OS_ANDROID)
  // Only count the page visit if it came from user browsing and only count it
  // once when cycling through a redirect chain.
  if (visit_source == SOURCE_BROWSED &&
      (transition & content::PAGE_TRANSITION_CHAIN_END) != 0) {
    RecordTopPageVisitStats(url);
  }
#endif

  // See if this URL is already in the DB.
  URLRow url_info(url);
  URLID url_id = db_->GetRowForURL(url, &url_info);
  if (url_id) {
    // Update of an existing row.
    if (content::PageTransitionStripQualifier(transition) !=
        content::PAGE_TRANSITION_RELOAD)
      url_info.set_visit_count(url_info.visit_count() + 1);
    if (typed_increment)
      url_info.set_typed_count(url_info.typed_count() + typed_increment);
    if (url_info.last_visit() < time)
      url_info.set_last_visit(time);

    // Only allow un-hiding of pages, never hiding.
    if (!new_hidden)
      url_info.set_hidden(false);

    db_->UpdateURLRow(url_id, url_info);
  } else {
    // Addition of a new row.
    url_info.set_visit_count(1);
    url_info.set_typed_count(typed_increment);
    url_info.set_last_visit(time);
    url_info.set_hidden(new_hidden);

    url_id = db_->AddURL(url_info);
    if (!url_id) {
      NOTREACHED() << "Adding URL failed.";
      return std::make_pair(0, 0);
    }
    url_info.id_ = url_id;
  }

  // Add the visit with the time to the database.
  VisitRow visit_info(url_id, time, referring_visit, transition, 0);
  VisitID visit_id = db_->AddVisit(&visit_info, visit_source);
  NotifyVisitObservers(visit_info);

  if (visit_info.visit_time < first_recorded_time_)
    first_recorded_time_ = visit_info.visit_time;

  // Broadcast a notification of the visit.
  if (visit_id) {
    if (typed_url_syncable_service_.get())
      typed_url_syncable_service_->OnUrlVisited(transition, &url_info);

    scoped_ptr<URLVisitedDetails> details(new URLVisitedDetails);
    details->transition = transition;
    details->row = url_info;
    details->visit_time = time;
    // TODO(meelapshah) Disabled due to potential PageCycler regression.
    // Re-enable this.
    // QueryRedirectsTo(url, &details->redirects);
    BroadcastNotifications(chrome::NOTIFICATION_HISTORY_URL_VISITED,
                           details.PassAs<HistoryDetails>());
  } else {
    VLOG(0) << "Failed to build visit insert statement:  "
            << "url_id = " << url_id;
  }

  return std::make_pair(url_id, visit_id);
}

void HistoryBackend::AddPagesWithDetails(const URLRows& urls,
                                         VisitSource visit_source) {
  if (!db_)
    return;

  scoped_ptr<URLsModifiedDetails> modified(new URLsModifiedDetails);
  for (URLRows::const_iterator i = urls.begin(); i != urls.end(); ++i) {
    DCHECK(!i->last_visit().is_null());

    // As of M37, we no longer maintain an archived database, ignore old visits.
    if (IsExpiredVisitTime(i->last_visit()))
      continue;

    URLRow existing_url;
    URLID url_id = db_->GetRowForURL(i->url(), &existing_url);
    if (!url_id) {
      // Add the page if it doesn't exist.
      url_id = db_->AddURL(*i);
      if (!url_id) {
        NOTREACHED() << "Could not add row to DB";
        return;
      }

      modified->changed_urls.push_back(*i);
      modified->changed_urls.back().set_id(url_id);  // i->id_ is likely 0.
    }

    // Sync code manages the visits itself.
    if (visit_source != SOURCE_SYNCED) {
      // Make up a visit to correspond to the last visit to the page.
      VisitRow visit_info(url_id, i->last_visit(), 0,
                          content::PageTransitionFromInt(
                              content::PAGE_TRANSITION_LINK |
                              content::PAGE_TRANSITION_CHAIN_START |
                              content::PAGE_TRANSITION_CHAIN_END), 0);
      if (!db_->AddVisit(&visit_info, visit_source)) {
        NOTREACHED() << "Adding visit failed.";
        return;
      }
      NotifyVisitObservers(visit_info);

      if (visit_info.visit_time < first_recorded_time_)
        first_recorded_time_ = visit_info.visit_time;
    }
  }

  if (typed_url_syncable_service_.get())
    typed_url_syncable_service_->OnUrlsModified(&modified->changed_urls);

  // Broadcast a notification for typed URLs that have been modified. This
  // will be picked up by the in-memory URL database on the main thread.
  //
  // TODO(brettw) bug 1140015: Add an "add page" notification so the history
  // views can keep in sync.
  BroadcastNotifications(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
                         modified.PassAs<HistoryDetails>());

  ScheduleCommit();
}

bool HistoryBackend::IsExpiredVisitTime(const base::Time& time) {
  return time < expirer_.GetCurrentExpirationTime();
}

void HistoryBackend::SetPageTitle(const GURL& url,
                                  const base::string16& title) {
  if (!db_)
    return;

  // Search for recent redirects which should get the same title. We make a
  // dummy list containing the exact URL visited if there are no redirects so
  // the processing below can be the same.
  history::RedirectList dummy_list;
  history::RedirectList* redirects;
  RedirectCache::iterator iter = recent_redirects_.Get(url);
  if (iter != recent_redirects_.end()) {
    redirects = &iter->second;

    // This redirect chain should have the destination URL as the last item.
    DCHECK(!redirects->empty());
    DCHECK(redirects->back() == url);
  } else {
    // No redirect chain stored, make up one containing the URL we want so we
    // can use the same logic below.
    dummy_list.push_back(url);
    redirects = &dummy_list;
  }

  scoped_ptr<URLsModifiedDetails> details(new URLsModifiedDetails);
  for (size_t i = 0; i < redirects->size(); i++) {
    URLRow row;
    URLID row_id = db_->GetRowForURL(redirects->at(i), &row);
    if (row_id && row.title() != title) {
      row.set_title(title);
      db_->UpdateURLRow(row_id, row);
      details->changed_urls.push_back(row);
    }
  }

  // Broadcast notifications for any URLs that have changed. This will
  // update the in-memory database and the InMemoryURLIndex.
  if (!details->changed_urls.empty()) {
    if (typed_url_syncable_service_.get())
      typed_url_syncable_service_->OnUrlsModified(&details->changed_urls);
    BroadcastNotifications(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
                           details.PassAs<HistoryDetails>());
    ScheduleCommit();
  }
}

void HistoryBackend::AddPageNoVisitForBookmark(const GURL& url,
                                               const base::string16& title) {
  if (!db_)
    return;

  URLRow url_info(url);
  URLID url_id = db_->GetRowForURL(url, &url_info);
  if (url_id) {
    // URL is already known, nothing to do.
    return;
  }

  if (!title.empty()) {
    url_info.set_title(title);
  } else {
    url_info.set_title(base::UTF8ToUTF16(url.spec()));
  }

  url_info.set_last_visit(Time::Now());
  // Mark the page hidden. If the user types it in, it'll unhide.
  url_info.set_hidden(true);

  db_->AddURL(url_info);
}

void HistoryBackend::IterateURLs(
    const scoped_refptr<visitedlink::VisitedLinkDelegate::URLEnumerator>&
    iterator) {
  if (db_) {
    HistoryDatabase::URLEnumerator e;
    if (db_->InitURLEnumeratorForEverything(&e)) {
      URLRow info;
      while (e.GetNextURL(&info)) {
        iterator->OnURL(info.url());
      }
      iterator->OnComplete(true);  // Success.
      return;
    }
  }
  iterator->OnComplete(false);  // Failure.
}

bool HistoryBackend::GetAllTypedURLs(URLRows* urls) {
  if (db_)
    return db_->GetAllTypedUrls(urls);
  return false;
}

bool HistoryBackend::GetVisitsForURL(URLID id, VisitVector* visits) {
  if (db_)
    return db_->GetVisitsForURL(id, visits);
  return false;
}

bool HistoryBackend::GetMostRecentVisitsForURL(URLID id,
                                               int max_visits,
                                               VisitVector* visits) {
  if (db_)
    return db_->GetMostRecentVisitsForURL(id, max_visits, visits);
  return false;
}

size_t HistoryBackend::UpdateURLs(const history::URLRows& urls) {
  if (!db_)
    return 0;

  scoped_ptr<URLsModifiedDetails> details(new URLsModifiedDetails);
  for (history::URLRows::const_iterator it = urls.begin(); it != urls.end();
       ++it) {
    DCHECK(it->id());
    if (db_->UpdateURLRow(it->id(), *it))
      details->changed_urls.push_back(*it);
  }

  // Broadcast notifications for any URLs that have actually been changed. This
  // will update the in-memory database and the InMemoryURLIndex.
  size_t num_updated_records = details->changed_urls.size();
  if (num_updated_records) {
    if (typed_url_syncable_service_)
      typed_url_syncable_service_->OnUrlsModified(&details->changed_urls);
    BroadcastNotifications(chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
                           details.PassAs<HistoryDetails>());
    ScheduleCommit();
  }
  return num_updated_records;
}

bool HistoryBackend::AddVisits(const GURL& url,
                               const std::vector<VisitInfo>& visits,
                               VisitSource visit_source) {
  if (db_) {
    for (std::vector<VisitInfo>::const_iterator visit = visits.begin();
         visit != visits.end(); ++visit) {
      if (!AddPageVisit(
              url, visit->first, 0, visit->second, visit_source).first) {
        return false;
      }
    }
    ScheduleCommit();
    return true;
  }
  return false;
}

bool HistoryBackend::RemoveVisits(const VisitVector& visits) {
  if (!db_)
    return false;

  expirer_.ExpireVisits(visits);
  ScheduleCommit();
  return true;
}

bool HistoryBackend::GetVisitsSource(const VisitVector& visits,
                                     VisitSourceMap* sources) {
  if (!db_)
    return false;

  db_->GetVisitsSource(visits, sources);
  return true;
}

bool HistoryBackend::GetURL(const GURL& url, history::URLRow* url_row) {
  if (db_)
    return db_->GetRowForURL(url, url_row) != 0;
  return false;
}

void HistoryBackend::QueryURL(const GURL& url,
                              bool want_visits,
                              QueryURLResult* result) {
  DCHECK(result);
  result->success = db_ && db_->GetRowForURL(url, &result->row);
  // Optionally query the visits.
  if (result->success && want_visits)
    db_->GetVisitsForURL(result->row.id(), &result->visits);
}

TypedUrlSyncableService* HistoryBackend::GetTypedUrlSyncableService() const {
  return typed_url_syncable_service_.get();
}

// Keyword visits --------------------------------------------------------------

void HistoryBackend::SetKeywordSearchTermsForURL(const GURL& url,
                                                 KeywordID keyword_id,
                                                 const base::string16& term) {
  if (!db_)
    return;

  // Get the ID for this URL.
  URLRow row;
  if (!db_->GetRowForURL(url, &row)) {
    // There is a small possibility the url was deleted before the keyword
    // was added. Ignore the request.
    return;
  }

  db_->SetKeywordSearchTermsForURL(row.id(), keyword_id, term);

  BroadcastNotifications(
      chrome::NOTIFICATION_HISTORY_KEYWORD_SEARCH_TERM_UPDATED,
      scoped_ptr<HistoryDetails>(
          new KeywordSearchUpdatedDetails(row, keyword_id, term)));
  ScheduleCommit();
}

void HistoryBackend::DeleteAllSearchTermsForKeyword(KeywordID keyword_id) {
  if (!db_)
    return;

  db_->DeleteAllSearchTermsForKeyword(keyword_id);
  ScheduleCommit();
}

void HistoryBackend::DeleteKeywordSearchTermForURL(const GURL& url) {
  if (!db_)
    return;

  URLID url_id = db_->GetRowForURL(url, NULL);
  if (!url_id)
    return;
  db_->DeleteKeywordSearchTermForURL(url_id);

  BroadcastNotifications(
      chrome::NOTIFICATION_HISTORY_KEYWORD_SEARCH_TERM_DELETED,
      scoped_ptr<HistoryDetails>(new KeywordSearchDeletedDetails(url_id)));
  ScheduleCommit();
}

void HistoryBackend::DeleteMatchingURLsForKeyword(KeywordID keyword_id,
                                                  const base::string16& term) {
  if (!db_)
    return;

  std::vector<KeywordSearchTermRow> rows;
  if (db_->GetKeywordSearchTermRows(term, &rows)) {
    std::vector<GURL> items_to_delete;
    URLRow row;
    for (std::vector<KeywordSearchTermRow>::iterator it = rows.begin();
         it != rows.end(); ++it) {
      if ((it->keyword_id == keyword_id) && db_->GetURLRow(it->url_id, &row))
        items_to_delete.push_back(row.url());
    }
    DeleteURLs(items_to_delete);
  }
}

// Downloads -------------------------------------------------------------------

uint32 HistoryBackend::GetNextDownloadId() {
  return db_ ? db_->GetNextDownloadId() : content::DownloadItem::kInvalidId;
}

// Get all the download entries from the database.
void HistoryBackend::QueryDownloads(std::vector<DownloadRow>* rows) {
  if (db_)
    db_->QueryDownloads(rows);
}

// Update a particular download entry.
void HistoryBackend::UpdateDownload(const history::DownloadRow& data) {
  if (!db_)
    return;
  db_->UpdateDownload(data);
  ScheduleCommit();
}

bool HistoryBackend::CreateDownload(const history::DownloadRow& history_info) {
  if (!db_)
    return false;
  bool success = db_->CreateDownload(history_info);
  ScheduleCommit();
  return success;
}

void HistoryBackend::RemoveDownloads(const std::set<uint32>& ids) {
  if (!db_)
    return;
  size_t downloads_count_before = db_->CountDownloads();
  base::TimeTicks started_removing = base::TimeTicks::Now();
  // HistoryBackend uses a long-running Transaction that is committed
  // periodically, so this loop doesn't actually hit the disk too hard.
  for (std::set<uint32>::const_iterator it = ids.begin();
       it != ids.end(); ++it) {
    db_->RemoveDownload(*it);
  }
  ScheduleCommit();
  base::TimeTicks finished_removing = base::TimeTicks::Now();
  size_t downloads_count_after = db_->CountDownloads();

  DCHECK_LE(downloads_count_after, downloads_count_before);
  if (downloads_count_after > downloads_count_before)
    return;
  size_t num_downloads_deleted = downloads_count_before - downloads_count_after;
  UMA_HISTOGRAM_COUNTS("Download.DatabaseRemoveDownloadsCount",
                        num_downloads_deleted);
  base::TimeDelta micros = (1000 * (finished_removing - started_removing));
  UMA_HISTOGRAM_TIMES("Download.DatabaseRemoveDownloadsTime", micros);
  if (num_downloads_deleted > 0) {
    UMA_HISTOGRAM_TIMES("Download.DatabaseRemoveDownloadsTimePerRecord",
                        (1000 * micros) / num_downloads_deleted);
  }
  DCHECK_GE(ids.size(), num_downloads_deleted);
  if (ids.size() < num_downloads_deleted)
    return;
  UMA_HISTOGRAM_COUNTS("Download.DatabaseRemoveDownloadsCountNotRemoved",
                        ids.size() - num_downloads_deleted);
}

void HistoryBackend::QueryHistory(const base::string16& text_query,
                                  const QueryOptions& options,
                                  QueryResults* query_results) {
  DCHECK(query_results);
  base::TimeTicks beginning_time = base::TimeTicks::Now();
  if (db_) {
    if (text_query.empty()) {
      // Basic history query for the main database.
      QueryHistoryBasic(options, query_results);
    } else {
      // Text history query.
      QueryHistoryText(text_query, options, query_results);
    }
  }
  UMA_HISTOGRAM_TIMES("History.QueryHistory",
                      TimeTicks::Now() - beginning_time);
}

// Basic time-based querying of history.
void HistoryBackend::QueryHistoryBasic(const QueryOptions& options,
                                       QueryResults* result) {
  // First get all visits.
  VisitVector visits;
  bool has_more_results = db_->GetVisibleVisitsInRange(options, &visits);
  DCHECK(static_cast<int>(visits.size()) <= options.EffectiveMaxCount());

  // Now add them and the URL rows to the results.
  URLResult url_result;
  for (size_t i = 0; i < visits.size(); i++) {
    const VisitRow visit = visits[i];

    // Add a result row for this visit, get the URL info from the DB.
    if (!db_->GetURLRow(visit.url_id, &url_result)) {
      VLOG(0) << "Failed to get id " << visit.url_id
              << " from history.urls.";
      continue;  // DB out of sync and URL doesn't exist, try to recover.
    }

    if (!url_result.url().is_valid()) {
      VLOG(0) << "Got invalid URL from history.urls with id "
              << visit.url_id << ":  "
              << url_result.url().possibly_invalid_spec();
      continue;  // Don't report invalid URLs in case of corruption.
    }

    url_result.set_visit_time(visit.visit_time);

    // Set whether the visit was blocked for a managed user by looking at the
    // transition type.
    url_result.set_blocked_visit(
        (visit.transition & content::PAGE_TRANSITION_BLOCKED) != 0);

    // We don't set any of the query-specific parts of the URLResult, since
    // snippets and stuff don't apply to basic querying.
    result->AppendURLBySwapping(&url_result);
  }

  if (!has_more_results && options.begin_time <= first_recorded_time_)
    result->set_reached_beginning(true);
}

// Text-based querying of history.
void HistoryBackend::QueryHistoryText(const base::string16& text_query,
                                      const QueryOptions& options,
                                      QueryResults* result) {
  URLRows text_matches;
  db_->GetTextMatches(text_query, &text_matches);

  std::vector<URLResult> matching_visits;
  VisitVector visits;    // Declare outside loop to prevent re-construction.
  for (size_t i = 0; i < text_matches.size(); i++) {
    const URLRow& text_match = text_matches[i];
    // Get all visits for given URL match.
    db_->GetVisibleVisitsForURL(text_match.id(), options, &visits);
    for (size_t j = 0; j < visits.size(); j++) {
      URLResult url_result(text_match);
      url_result.set_visit_time(visits[j].visit_time);
      matching_visits.push_back(url_result);
    }
  }

  std::sort(matching_visits.begin(), matching_visits.end(),
            URLResult::CompareVisitTime);

  size_t max_results = options.max_count == 0 ?
      std::numeric_limits<size_t>::max() : static_cast<int>(options.max_count);
  for (std::vector<URLResult>::iterator it = matching_visits.begin();
       it != matching_visits.end() && result->size() < max_results; ++it) {
    result->AppendURLBySwapping(&(*it));
  }

  if (matching_visits.size() == result->size() &&
      options.begin_time <= first_recorded_time_)
    result->set_reached_beginning(true);
}

void HistoryBackend::QueryRedirectsFrom(const GURL& from_url,
                                        RedirectList* redirects) {
  redirects->clear();
  if (!db_)
    return;

  URLID from_url_id = db_->GetRowForURL(from_url, NULL);
  VisitID cur_visit = db_->GetMostRecentVisitForURL(from_url_id, NULL);
  if (!cur_visit)
    return;  // No visits for URL.

  GetRedirectsFromSpecificVisit(cur_visit, redirects);
}

void HistoryBackend::QueryRedirectsTo(const GURL& to_url,
                                      RedirectList* redirects) {
  redirects->clear();
  if (!db_)
    return;

  URLID to_url_id = db_->GetRowForURL(to_url, NULL);
  VisitID cur_visit = db_->GetMostRecentVisitForURL(to_url_id, NULL);
  if (!cur_visit)
    return;  // No visits for URL.

  GetRedirectsToSpecificVisit(cur_visit, redirects);
}

void HistoryBackend::GetVisibleVisitCountToHost(
    const GURL& url,
    VisibleVisitCountToHostResult* result) {
  result->count = 0;
  result->success = db_.get() &&
                    db_->GetVisibleVisitCountToHost(
                        url, &result->count, &result->first_visit);
}

void HistoryBackend::QueryMostVisitedURLs(int result_count,
                                          int days_back,
                                          MostVisitedURLList* result) {
  if (!db_)
    return;

  ScopedVector<PageUsageData> data;
  db_->QuerySegmentUsage(
      base::Time::Now() - base::TimeDelta::FromDays(days_back),
      result_count,
      &data.get());

  for (size_t i = 0; i < data.size(); ++i) {
    PageUsageData* current_data = data[i];
    RedirectList redirects;
    QueryRedirectsFrom(current_data->GetURL(), &redirects);
    MostVisitedURL url = MakeMostVisitedURL(*current_data, redirects);
    result->push_back(url);
  }
}

void HistoryBackend::QueryFilteredURLs(int result_count,
                                       const history::VisitFilter& filter,
                                       bool extended_info,
                                       history::FilteredURLList* result) {
  DCHECK(result);
  base::Time request_start = base::Time::Now();

  result->clear();
  if (!db_) {
    // No History Database - return an empty list.
    return;
  }

  VisitVector visits;
  db_->GetDirectVisitsDuringTimes(filter, 0, &visits);

  std::map<URLID, double> score_map;
  for (size_t i = 0; i < visits.size(); ++i) {
    score_map[visits[i].url_id] += filter.GetVisitScore(visits[i]);
  }

  // TODO(georgey): experiment with visit_segment database granularity (it is
  // currently 24 hours) to use it directly instead of using visits database,
  // which is considerably slower.
  ScopedVector<PageUsageData> data;
  data.reserve(score_map.size());
  for (std::map<URLID, double>::iterator it = score_map.begin();
       it != score_map.end(); ++it) {
    PageUsageData* pud = new PageUsageData(it->first);
    pud->SetScore(it->second);
    data.push_back(pud);
  }

  // Limit to the top |result_count| results.
  std::sort(data.begin(), data.end(), PageUsageData::Predicate);
  if (result_count && implicit_cast<int>(data.size()) > result_count)
    data.resize(result_count);

  for (size_t i = 0; i < data.size(); ++i) {
    URLRow info;
    if (db_->GetURLRow(data[i]->GetID(), &info)) {
      data[i]->SetURL(info.url());
      data[i]->SetTitle(info.title());
    }
  }

  for (size_t i = 0; i < data.size(); ++i) {
    PageUsageData* current_data = data[i];
    FilteredURL url(*current_data);

    if (extended_info) {
      VisitVector visits;
      db_->GetVisitsForURL(current_data->GetID(), &visits);
      if (visits.size() > 0) {
        url.extended_info.total_visits = visits.size();
        for (size_t i = 0; i < visits.size(); ++i) {
          url.extended_info.duration_opened +=
              visits[i].visit_duration.InSeconds();
          if (visits[i].visit_time > url.extended_info.last_visit_time) {
            url.extended_info.last_visit_time = visits[i].visit_time;
          }
        }
        // TODO(macourteau): implement the url.extended_info.visits stat.
      }
    }
    result->push_back(url);
  }

  int delta_time = std::max(1, std::min(999,
      static_cast<int>((base::Time::Now() - request_start).InMilliseconds())));
  STATIC_HISTOGRAM_POINTER_BLOCK(
      "NewTabPage.SuggestedSitesLoadTime",
      Add(delta_time),
      base::LinearHistogram::FactoryGet("NewTabPage.SuggestedSitesLoadTime",
          1, 1000, 100, base::Histogram::kUmaTargetedHistogramFlag));
}

void HistoryBackend::GetRedirectsFromSpecificVisit(
    VisitID cur_visit, history::RedirectList* redirects) {
  // Follow any redirects from the given visit and add them to the list.
  // It *should* be impossible to get a circular chain here, but we check
  // just in case to avoid infinite loops.
  GURL cur_url;
  std::set<VisitID> visit_set;
  visit_set.insert(cur_visit);
  while (db_->GetRedirectFromVisit(cur_visit, &cur_visit, &cur_url)) {
    if (visit_set.find(cur_visit) != visit_set.end()) {
      NOTREACHED() << "Loop in visit chain, giving up";
      return;
    }
    visit_set.insert(cur_visit);
    redirects->push_back(cur_url);
  }
}

void HistoryBackend::GetRedirectsToSpecificVisit(
    VisitID cur_visit,
    history::RedirectList* redirects) {
  // Follow redirects going to cur_visit. These are added to |redirects| in
  // the order they are found. If a redirect chain looks like A -> B -> C and
  // |cur_visit| = C, redirects will be {B, A} in that order.
  if (!db_)
    return;

  GURL cur_url;
  std::set<VisitID> visit_set;
  visit_set.insert(cur_visit);
  while (db_->GetRedirectToVisit(cur_visit, &cur_visit, &cur_url)) {
    if (visit_set.find(cur_visit) != visit_set.end()) {
      NOTREACHED() << "Loop in visit chain, giving up";
      return;
    }
    visit_set.insert(cur_visit);
    redirects->push_back(cur_url);
  }
}

void HistoryBackend::ScheduleAutocomplete(const base::Callback<
    void(history::HistoryBackend*, history::URLDatabase*)>& callback) {
  callback.Run(this, db_.get());
}

void HistoryBackend::DeleteFTSIndexDatabases() {
  // Find files on disk matching the text databases file pattern so we can
  // quickly test for and delete them.
  base::FilePath::StringType filepattern =
      FILE_PATH_LITERAL("History Index *");
  base::FileEnumerator enumerator(
      history_dir_, false, base::FileEnumerator::FILES, filepattern);
  int num_databases_deleted = 0;
  base::FilePath current_file;
  while (!(current_file = enumerator.Next()).empty()) {
    if (sql::Connection::Delete(current_file))
      num_databases_deleted++;
  }
  UMA_HISTOGRAM_COUNTS("History.DeleteFTSIndexDatabases",
                       num_databases_deleted);
}

void HistoryBackend::GetFavicons(
    const std::vector<GURL>& icon_urls,
    int icon_types,
    const std::vector<int>& desired_sizes,
    std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results) {
  UpdateFaviconMappingsAndFetchImpl(NULL, icon_urls, icon_types, desired_sizes,
                                    bitmap_results);
}

void HistoryBackend::GetLargestFaviconForURL(
    const GURL& page_url,
    const std::vector<int>& icon_types,
    int minimum_size_in_pixels,
    favicon_base::FaviconRawBitmapResult* favicon_bitmap_result) {
  DCHECK(favicon_bitmap_result);

  if (!db_ || !thumbnail_db_)
    return;

  TimeTicks beginning_time = TimeTicks::Now();

  std::vector<IconMapping> icon_mappings;
  if (!thumbnail_db_->GetIconMappingsForPageURL(page_url, &icon_mappings) ||
      icon_mappings.empty())
    return;

  int required_icon_types = 0;
  for (std::vector<int>::const_iterator i = icon_types.begin();
       i != icon_types.end(); ++i) {
    required_icon_types |= *i;
  }

  // Find the largest bitmap for each IconType placing in
  // |largest_favicon_bitmaps|.
  std::map<favicon_base::IconType, FaviconBitmap> largest_favicon_bitmaps;
  for (std::vector<IconMapping>::const_iterator i = icon_mappings.begin();
       i != icon_mappings.end(); ++i) {
    if (!(i->icon_type & required_icon_types))
      continue;
    std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
    thumbnail_db_->GetFaviconBitmapIDSizes(i->icon_id, &bitmap_id_sizes);
    FaviconBitmap& largest = largest_favicon_bitmaps[i->icon_type];
    for (std::vector<FaviconBitmapIDSize>::const_iterator j =
             bitmap_id_sizes.begin(); j != bitmap_id_sizes.end(); ++j) {
      if (largest.bitmap_id == 0 ||
          (largest.pixel_size.width() < j->pixel_size.width() &&
           largest.pixel_size.height() < j->pixel_size.height())) {
        largest.icon_id = i->icon_id;
        largest.bitmap_id = j->bitmap_id;
        largest.pixel_size = j->pixel_size;
      }
    }
  }
  if (largest_favicon_bitmaps.empty())
    return;

  // Find an icon which is larger than minimum_size_in_pixels in the order of
  // icon_types.
  FaviconBitmap largest_icon;
  for (std::vector<int>::const_iterator t = icon_types.begin();
       t != icon_types.end(); ++t) {
    for (std::map<favicon_base::IconType, FaviconBitmap>::const_iterator f =
             largest_favicon_bitmaps.begin();
         f != largest_favicon_bitmaps.end();
         ++f) {
      if (f->first & *t &&
          (largest_icon.bitmap_id == 0 ||
           (largest_icon.pixel_size.height() < f->second.pixel_size.height() &&
            largest_icon.pixel_size.width() < f->second.pixel_size.width()))) {
        largest_icon = f->second;
      }
    }
    if (largest_icon.pixel_size.width() > minimum_size_in_pixels &&
        largest_icon.pixel_size.height() > minimum_size_in_pixels)
      break;
  }

  GURL icon_url;
  favicon_base::IconType icon_type;
  if (!thumbnail_db_->GetFaviconHeader(largest_icon.icon_id, &icon_url,
                                       &icon_type)) {
    return;
  }

  base::Time last_updated;
  favicon_base::FaviconRawBitmapResult bitmap_result;
  bitmap_result.icon_url = icon_url;
  bitmap_result.icon_type = icon_type;
  if (!thumbnail_db_->GetFaviconBitmap(largest_icon.bitmap_id,
                                       &last_updated,
                                       &bitmap_result.bitmap_data,
                                       &bitmap_result.pixel_size)) {
    return;
  }

  bitmap_result.expired = (Time::Now() - last_updated) >
      TimeDelta::FromDays(kFaviconRefetchDays);
  if (bitmap_result.is_valid())
    *favicon_bitmap_result = bitmap_result;

  HISTOGRAM_TIMES("History.GetLargestFaviconForURL",
                  TimeTicks::Now() - beginning_time);
}

void HistoryBackend::GetFaviconsForURL(
    const GURL& page_url,
    int icon_types,
    const std::vector<int>& desired_sizes,
    std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results) {
  DCHECK(bitmap_results);
  GetFaviconsFromDB(page_url, icon_types, desired_sizes, bitmap_results);
}

void HistoryBackend::GetFaviconForID(
    favicon_base::FaviconID favicon_id,
    int desired_size,
    std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results) {
  std::vector<favicon_base::FaviconID> favicon_ids;
  favicon_ids.push_back(favicon_id);
  std::vector<int> desired_sizes;
  desired_sizes.push_back(desired_size);

  // Get results from DB.
  GetFaviconBitmapResultsForBestMatch(favicon_ids,
                                      desired_sizes,
                                      bitmap_results);
}

void HistoryBackend::UpdateFaviconMappingsAndFetch(
    const GURL& page_url,
    const std::vector<GURL>& icon_urls,
    int icon_types,
    const std::vector<int>& desired_sizes,
    std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results) {
  UpdateFaviconMappingsAndFetchImpl(&page_url, icon_urls, icon_types,
                                    desired_sizes, bitmap_results);
}

void HistoryBackend::MergeFavicon(
    const GURL& page_url,
    const GURL& icon_url,
    favicon_base::IconType icon_type,
    scoped_refptr<base::RefCountedMemory> bitmap_data,
    const gfx::Size& pixel_size) {
  if (!thumbnail_db_ || !db_)
    return;

  favicon_base::FaviconID favicon_id =
      thumbnail_db_->GetFaviconIDForFaviconURL(icon_url, icon_type, NULL);

  if (!favicon_id) {
    // There is no favicon at |icon_url|, create it.
    favicon_id = thumbnail_db_->AddFavicon(icon_url, icon_type);
  }

  std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
  thumbnail_db_->GetFaviconBitmapIDSizes(favicon_id, &bitmap_id_sizes);

  // If there is already a favicon bitmap of |pixel_size| at |icon_url|,
  // replace it.
  bool bitmap_identical = false;
  bool replaced_bitmap = false;
  for (size_t i = 0; i < bitmap_id_sizes.size(); ++i) {
    if (bitmap_id_sizes[i].pixel_size == pixel_size) {
      if (IsFaviconBitmapDataEqual(bitmap_id_sizes[i].bitmap_id, bitmap_data)) {
        thumbnail_db_->SetFaviconBitmapLastUpdateTime(
            bitmap_id_sizes[i].bitmap_id, base::Time::Now());
        bitmap_identical = true;
      } else {
        thumbnail_db_->SetFaviconBitmap(bitmap_id_sizes[i].bitmap_id,
            bitmap_data, base::Time::Now());
        replaced_bitmap = true;
      }
      break;
    }
  }

  // Create a vector of the pixel sizes of the favicon bitmaps currently at
  // |icon_url|.
  std::vector<gfx::Size> favicon_sizes;
  for (size_t i = 0; i < bitmap_id_sizes.size(); ++i)
    favicon_sizes.push_back(bitmap_id_sizes[i].pixel_size);

  if (!replaced_bitmap && !bitmap_identical) {
    // Set the preexisting favicon bitmaps as expired as the preexisting favicon
    // bitmaps are not consistent with the merged in data.
    thumbnail_db_->SetFaviconOutOfDate(favicon_id);

    // Delete an arbitrary favicon bitmap to avoid going over the limit of
    // |kMaxFaviconBitmapsPerIconURL|.
    if (bitmap_id_sizes.size() >= kMaxFaviconBitmapsPerIconURL) {
      thumbnail_db_->DeleteFaviconBitmap(bitmap_id_sizes[0].bitmap_id);
      favicon_sizes.erase(favicon_sizes.begin());
    }
    thumbnail_db_->AddFaviconBitmap(favicon_id, bitmap_data, base::Time::Now(),
                                    pixel_size);
    favicon_sizes.push_back(pixel_size);
  }

  // A site may have changed the favicons that it uses for |page_url|.
  // Example Scenario:
  //   page_url = news.google.com
  //   Initial State: www.google.com/favicon.ico 16x16, 32x32
  //   MergeFavicon(news.google.com, news.google.com/news_specific.ico, ...,
  //                ..., 16x16)
  //
  // Difficulties:
  // 1. Sync requires that a call to GetFaviconsForURL() returns the
  //    |bitmap_data| passed into MergeFavicon().
  //    - It is invalid for the 16x16 bitmap for www.google.com/favicon.ico to
  //      stay mapped to news.google.com because it would be unclear which 16x16
  //      bitmap should be returned via GetFaviconsForURL().
  //
  // 2. www.google.com/favicon.ico may be mapped to more than just
  //    news.google.com (eg www.google.com).
  //    - The 16x16 bitmap cannot be deleted from www.google.com/favicon.ico
  //
  // To resolve these problems, we copy all of the favicon bitmaps previously
  // mapped to news.google.com (|page_url|) and add them to the favicon at
  // news.google.com/news_specific.ico (|icon_url|). The favicon sizes for
  // |icon_url| are set to default to indicate that |icon_url| has incomplete
  // / incorrect data.
  // Difficulty 1: All but news.google.com/news_specific.ico are unmapped from
  //              news.google.com
  // Difficulty 2: The favicon bitmaps for www.google.com/favicon.ico are not
  //               modified.

  std::vector<IconMapping> icon_mappings;
  thumbnail_db_->GetIconMappingsForPageURL(page_url, icon_type, &icon_mappings);

  // Copy the favicon bitmaps mapped to |page_url| to the favicon at |icon_url|
  // till the limit of |kMaxFaviconBitmapsPerIconURL| is reached.
  for (size_t i = 0; i < icon_mappings.size(); ++i) {
    if (favicon_sizes.size() >= kMaxFaviconBitmapsPerIconURL)
      break;

    if (icon_mappings[i].icon_url == icon_url)
      continue;

    std::vector<FaviconBitmap> bitmaps_to_copy;
    thumbnail_db_->GetFaviconBitmaps(icon_mappings[i].icon_id,
                                     &bitmaps_to_copy);
    for (size_t j = 0; j < bitmaps_to_copy.size(); ++j) {
      // Do not add a favicon bitmap at a pixel size for which there is already
      // a favicon bitmap mapped to |icon_url|. The one there is more correct
      // and having multiple equally sized favicon bitmaps for |page_url| is
      // ambiguous in terms of GetFaviconsForURL().
      std::vector<gfx::Size>::iterator it = std::find(favicon_sizes.begin(),
          favicon_sizes.end(), bitmaps_to_copy[j].pixel_size);
      if (it != favicon_sizes.end())
        continue;

      // Add the favicon bitmap as expired as it is not consistent with the
      // merged in data.
      thumbnail_db_->AddFaviconBitmap(favicon_id,
          bitmaps_to_copy[j].bitmap_data, base::Time(),
          bitmaps_to_copy[j].pixel_size);
      favicon_sizes.push_back(bitmaps_to_copy[j].pixel_size);

      if (favicon_sizes.size() >= kMaxFaviconBitmapsPerIconURL)
        break;
    }
  }

  // Update the favicon mappings such that only |icon_url| is mapped to
  // |page_url|.
  bool mapping_changed = false;
  if (icon_mappings.size() != 1 || icon_mappings[0].icon_url != icon_url) {
    std::vector<favicon_base::FaviconID> favicon_ids;
    favicon_ids.push_back(favicon_id);
    SetFaviconMappingsForPageAndRedirects(page_url, icon_type, favicon_ids);
    mapping_changed = true;
  }

  if (mapping_changed || !bitmap_identical)
    SendFaviconChangedNotificationForPageAndRedirects(page_url);
  ScheduleCommit();
}

void HistoryBackend::SetFavicons(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    const std::vector<favicon_base::FaviconRawBitmapData>&
        favicon_bitmap_data) {
  if (!thumbnail_db_ || !db_)
    return;

  DCHECK(ValidateSetFaviconsParams(favicon_bitmap_data));

  // Build map of FaviconRawBitmapData for each icon url.
  typedef std::map<GURL, std::vector<favicon_base::FaviconRawBitmapData> >
      BitmapDataByIconURL;
  BitmapDataByIconURL grouped_by_icon_url;
  for (size_t i = 0; i < favicon_bitmap_data.size(); ++i) {
    const GURL& icon_url = favicon_bitmap_data[i].icon_url;
    grouped_by_icon_url[icon_url].push_back(favicon_bitmap_data[i]);
  }

  // Track whether the method modifies or creates any favicon bitmaps, favicons
  // or icon mappings.
  bool data_modified = false;

  std::vector<favicon_base::FaviconID> icon_ids;
  for (BitmapDataByIconURL::const_iterator it = grouped_by_icon_url.begin();
       it != grouped_by_icon_url.end(); ++it) {
    const GURL& icon_url = it->first;
    favicon_base::FaviconID icon_id =
        thumbnail_db_->GetFaviconIDForFaviconURL(icon_url, icon_type, NULL);

    if (!icon_id) {
      // TODO(pkotwicz): Remove the favicon sizes attribute from
      // ThumbnailDatabase::AddFavicon().
      icon_id = thumbnail_db_->AddFavicon(icon_url, icon_type);
      data_modified = true;
    }
    icon_ids.push_back(icon_id);

    if (!data_modified)
      SetFaviconBitmaps(icon_id, it->second, &data_modified);
    else
      SetFaviconBitmaps(icon_id, it->second, NULL);
  }

  data_modified |=
    SetFaviconMappingsForPageAndRedirects(page_url, icon_type, icon_ids);

  if (data_modified) {
    // Send notification to the UI as an icon mapping, favicon, or favicon
    // bitmap was changed by this function.
    SendFaviconChangedNotificationForPageAndRedirects(page_url);
  }
  ScheduleCommit();
}

void HistoryBackend::SetFaviconsOutOfDateForPage(const GURL& page_url) {
  std::vector<IconMapping> icon_mappings;

  if (!thumbnail_db_ ||
      !thumbnail_db_->GetIconMappingsForPageURL(page_url,
                                                &icon_mappings))
    return;

  for (std::vector<IconMapping>::iterator m = icon_mappings.begin();
       m != icon_mappings.end(); ++m) {
    thumbnail_db_->SetFaviconOutOfDate(m->icon_id);
  }
  ScheduleCommit();
}

void HistoryBackend::CloneFavicons(const GURL& old_page_url,
                                   const GURL& new_page_url) {
  if (!thumbnail_db_)
    return;

  // Prevent cross-domain cloning.
  if (old_page_url.GetOrigin() != new_page_url.GetOrigin())
    return;

  thumbnail_db_->CloneIconMappings(old_page_url, new_page_url);
  ScheduleCommit();
}

void HistoryBackend::SetImportedFavicons(
    const std::vector<ImportedFaviconUsage>& favicon_usage) {
  if (!db_ || !thumbnail_db_)
    return;

  Time now = Time::Now();

  // Track all URLs that had their favicons set or updated.
  std::set<GURL> favicons_changed;

  for (size_t i = 0; i < favicon_usage.size(); i++) {
    favicon_base::FaviconID favicon_id =
        thumbnail_db_->GetFaviconIDForFaviconURL(
            favicon_usage[i].favicon_url, favicon_base::FAVICON, NULL);
    if (!favicon_id) {
      // This favicon doesn't exist yet, so we create it using the given data.
      // TODO(pkotwicz): Pass in real pixel size.
      favicon_id = thumbnail_db_->AddFavicon(
          favicon_usage[i].favicon_url,
          favicon_base::FAVICON,
          new base::RefCountedBytes(favicon_usage[i].png_data),
          now,
          gfx::Size());
    }

    // Save the mapping from all the URLs to the favicon.
    HistoryClient* history_client = GetHistoryClient();
    for (std::set<GURL>::const_iterator url = favicon_usage[i].urls.begin();
         url != favicon_usage[i].urls.end(); ++url) {
      URLRow url_row;
      if (!db_->GetRowForURL(*url, &url_row)) {
        // If the URL is present as a bookmark, add the url in history to
        // save the favicon mapping. This will match with what history db does
        // for regular bookmarked URLs with favicons - when history db is
        // cleaned, we keep an entry in the db with 0 visits as long as that
        // url is bookmarked.
        if (history_client && history_client->IsBookmarked(*url)) {
          URLRow url_info(*url);
          url_info.set_visit_count(0);
          url_info.set_typed_count(0);
          url_info.set_last_visit(base::Time());
          url_info.set_hidden(false);
          db_->AddURL(url_info);
          thumbnail_db_->AddIconMapping(*url, favicon_id);
          favicons_changed.insert(*url);
        }
      } else {
        if (!thumbnail_db_->GetIconMappingsForPageURL(
                *url, favicon_base::FAVICON, NULL)) {
          // URL is present in history, update the favicon *only* if it is not
          // set already.
          thumbnail_db_->AddIconMapping(*url, favicon_id);
          favicons_changed.insert(*url);
        }
      }
    }
  }

  if (!favicons_changed.empty()) {
    // Send the notification about the changed favicon URLs.
    scoped_ptr<FaviconChangedDetails> changed_details(
        new FaviconChangedDetails);
    changed_details->urls.swap(favicons_changed);
    BroadcastNotifications(chrome::NOTIFICATION_FAVICON_CHANGED,
                           changed_details.PassAs<HistoryDetails>());
  }
}

void HistoryBackend::UpdateFaviconMappingsAndFetchImpl(
    const GURL* page_url,
    const std::vector<GURL>& icon_urls,
    int icon_types,
    const std::vector<int>& desired_sizes,
    std::vector<favicon_base::FaviconRawBitmapResult>* bitmap_results) {
  // If |page_url| is specified, |icon_types| must be either a single icon
  // type or icon types which are equivalent.
  DCHECK(!page_url || icon_types == favicon_base::FAVICON ||
         icon_types == favicon_base::TOUCH_ICON ||
         icon_types == favicon_base::TOUCH_PRECOMPOSED_ICON ||
         icon_types ==
             (favicon_base::TOUCH_ICON | favicon_base::TOUCH_PRECOMPOSED_ICON));
  bitmap_results->clear();

  if (!thumbnail_db_) {
    return;
  }

  std::vector<favicon_base::FaviconID> favicon_ids;

  // The icon type for which the mappings will the updated and data will be
  // returned.
  favicon_base::IconType selected_icon_type = favicon_base::INVALID_ICON;

  for (size_t i = 0; i < icon_urls.size(); ++i) {
    const GURL& icon_url = icon_urls[i];
    favicon_base::IconType icon_type_out;
    const favicon_base::FaviconID favicon_id =
        thumbnail_db_->GetFaviconIDForFaviconURL(
            icon_url, icon_types, &icon_type_out);

    if (favicon_id) {
      // Return and update icon mappings only for the largest icon type. As
      // |icon_urls| is not sorted in terms of icon type, clear |favicon_ids|
      // if an |icon_url| with a larger icon type is found.
      if (icon_type_out > selected_icon_type) {
        selected_icon_type = icon_type_out;
        favicon_ids.clear();
      }
      if (icon_type_out == selected_icon_type)
        favicon_ids.push_back(favicon_id);
    }
  }

  if (page_url && !favicon_ids.empty()) {
    bool mappings_updated =
        SetFaviconMappingsForPageAndRedirects(*page_url, selected_icon_type,
                                              favicon_ids);
    if (mappings_updated) {
      SendFaviconChangedNotificationForPageAndRedirects(*page_url);
      ScheduleCommit();
    }
  }

  GetFaviconBitmapResultsForBestMatch(favicon_ids, desired_sizes,
      bitmap_results);
}

void HistoryBackend::SetFaviconBitmaps(
    favicon_base::FaviconID icon_id,
    const std::vector<favicon_base::FaviconRawBitmapData>& favicon_bitmap_data,
    bool* favicon_bitmaps_changed) {
  if (favicon_bitmaps_changed)
    *favicon_bitmaps_changed = false;

  std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
  thumbnail_db_->GetFaviconBitmapIDSizes(icon_id, &bitmap_id_sizes);

  std::vector<favicon_base::FaviconRawBitmapData> to_add = favicon_bitmap_data;

  for (size_t i = 0; i < bitmap_id_sizes.size(); ++i) {
    const gfx::Size& pixel_size = bitmap_id_sizes[i].pixel_size;
    std::vector<favicon_base::FaviconRawBitmapData>::iterator match_it =
        to_add.end();
    for (std::vector<favicon_base::FaviconRawBitmapData>::iterator it =
             to_add.begin();
         it != to_add.end();
         ++it) {
      if (it->pixel_size == pixel_size) {
        match_it = it;
        break;
      }
    }

    FaviconBitmapID bitmap_id = bitmap_id_sizes[i].bitmap_id;
    if (match_it == to_add.end()) {
      thumbnail_db_->DeleteFaviconBitmap(bitmap_id);

      if (favicon_bitmaps_changed)
        *favicon_bitmaps_changed = true;
    } else {
      if (favicon_bitmaps_changed &&
          !*favicon_bitmaps_changed &&
          IsFaviconBitmapDataEqual(bitmap_id, match_it->bitmap_data)) {
        thumbnail_db_->SetFaviconBitmapLastUpdateTime(
            bitmap_id, base::Time::Now());
      } else {
        thumbnail_db_->SetFaviconBitmap(bitmap_id, match_it->bitmap_data,
            base::Time::Now());

        if (favicon_bitmaps_changed)
          *favicon_bitmaps_changed = true;
      }
      to_add.erase(match_it);
    }
  }

  for (size_t i = 0; i < to_add.size(); ++i) {
    thumbnail_db_->AddFaviconBitmap(icon_id, to_add[i].bitmap_data,
        base::Time::Now(), to_add[i].pixel_size);

    if (favicon_bitmaps_changed)
      *favicon_bitmaps_changed = true;
  }
}

bool HistoryBackend::ValidateSetFaviconsParams(const std::vector<
    favicon_base::FaviconRawBitmapData>& favicon_bitmap_data) const {
  typedef std::map<GURL, size_t> BitmapsPerIconURL;
  BitmapsPerIconURL num_bitmaps_per_icon_url;
  for (size_t i = 0; i < favicon_bitmap_data.size(); ++i) {
    if (!favicon_bitmap_data[i].bitmap_data.get())
      return false;

    const GURL& icon_url = favicon_bitmap_data[i].icon_url;
    if (!num_bitmaps_per_icon_url.count(icon_url))
      num_bitmaps_per_icon_url[icon_url] = 1u;
    else
      ++num_bitmaps_per_icon_url[icon_url];
  }

  if (num_bitmaps_per_icon_url.size() > kMaxFaviconsPerPage)
    return false;

  for (BitmapsPerIconURL::const_iterator it = num_bitmaps_per_icon_url.begin();
       it != num_bitmaps_per_icon_url.end(); ++it) {
    if (it->second > kMaxFaviconBitmapsPerIconURL)
      return false;
  }
  return true;
}

bool HistoryBackend::IsFaviconBitmapDataEqual(
    FaviconBitmapID bitmap_id,
    const scoped_refptr<base::RefCountedMemory>& new_bitmap_data) {
  if (!new_bitmap_data.get())
    return false;

  scoped_refptr<base::RefCountedMemory> original_bitmap_data;
  thumbnail_db_->GetFaviconBitmap(bitmap_id,
                                  NULL,
                                  &original_bitmap_data,
                                  NULL);
  return new_bitmap_data->Equals(original_bitmap_data);
}

bool HistoryBackend::GetFaviconsFromDB(
    const GURL& page_url,
    int icon_types,
    const std::vector<int>& desired_sizes,
    std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results) {
  DCHECK(favicon_bitmap_results);
  favicon_bitmap_results->clear();

  if (!db_ || !thumbnail_db_)
    return false;

  // Time the query.
  TimeTicks beginning_time = TimeTicks::Now();

  // Get FaviconIDs for |page_url| and one of |icon_types|.
  std::vector<IconMapping> icon_mappings;
  thumbnail_db_->GetIconMappingsForPageURL(page_url, icon_types,
                                           &icon_mappings);
  std::vector<favicon_base::FaviconID> favicon_ids;
  for (size_t i = 0; i < icon_mappings.size(); ++i)
    favicon_ids.push_back(icon_mappings[i].icon_id);

  // Populate |favicon_bitmap_results| and |icon_url_sizes|.
  bool success = GetFaviconBitmapResultsForBestMatch(favicon_ids,
      desired_sizes, favicon_bitmap_results);
  UMA_HISTOGRAM_TIMES("History.GetFavIconFromDB",  // historical name
                      TimeTicks::Now() - beginning_time);
  return success && !favicon_bitmap_results->empty();
}

bool HistoryBackend::GetFaviconBitmapResultsForBestMatch(
    const std::vector<favicon_base::FaviconID>& candidate_favicon_ids,
    const std::vector<int>& desired_sizes,
    std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results) {
  favicon_bitmap_results->clear();

  if (candidate_favicon_ids.empty())
    return true;

  // Find the FaviconID and the FaviconBitmapIDs which best match
  // |desired_size_in_dip| and |desired_scale_factors|.
  // TODO(pkotwicz): Select bitmap results from multiple favicons once
  // content::FaviconStatus supports multiple icon URLs.
  favicon_base::FaviconID best_favicon_id = 0;
  std::vector<FaviconBitmapID> best_bitmap_ids;
  float highest_score = kSelectFaviconFramesInvalidScore;
  for (size_t i = 0; i < candidate_favicon_ids.size(); ++i) {
    std::vector<FaviconBitmapIDSize> bitmap_id_sizes;
    thumbnail_db_->GetFaviconBitmapIDSizes(candidate_favicon_ids[i],
                                           &bitmap_id_sizes);

    // Build vector of gfx::Size from |bitmap_id_sizes|.
    std::vector<gfx::Size> sizes;
    for (size_t j = 0; j < bitmap_id_sizes.size(); ++j)
      sizes.push_back(bitmap_id_sizes[j].pixel_size);

    std::vector<size_t> candidate_bitmap_indices;
    float score = 0;
    SelectFaviconFrameIndices(sizes,
                              desired_sizes,
                              &candidate_bitmap_indices,
                              &score);
    if (score > highest_score) {
      highest_score = score;
      best_favicon_id = candidate_favicon_ids[i],
      best_bitmap_ids.clear();
      for (size_t j = 0; j < candidate_bitmap_indices.size(); ++j) {
        size_t candidate_index = candidate_bitmap_indices[j];
        best_bitmap_ids.push_back(
            bitmap_id_sizes[candidate_index].bitmap_id);
      }
    }
  }

  // Construct FaviconRawBitmapResults from |best_favicon_id| and
  // |best_bitmap_ids|.
  GURL icon_url;
  favicon_base::IconType icon_type;
  if (!thumbnail_db_->GetFaviconHeader(best_favicon_id, &icon_url,
                                       &icon_type)) {
    return false;
  }

  for (size_t i = 0; i < best_bitmap_ids.size(); ++i) {
    base::Time last_updated;
    favicon_base::FaviconRawBitmapResult bitmap_result;
    bitmap_result.icon_url = icon_url;
    bitmap_result.icon_type = icon_type;
    if (!thumbnail_db_->GetFaviconBitmap(best_bitmap_ids[i],
                                         &last_updated,
                                         &bitmap_result.bitmap_data,
                                         &bitmap_result.pixel_size)) {
      return false;
    }

    bitmap_result.expired = (Time::Now() - last_updated) >
        TimeDelta::FromDays(kFaviconRefetchDays);
    if (bitmap_result.is_valid())
      favicon_bitmap_results->push_back(bitmap_result);
  }
  return true;
}

bool HistoryBackend::SetFaviconMappingsForPageAndRedirects(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    const std::vector<favicon_base::FaviconID>& icon_ids) {
  if (!thumbnail_db_)
    return false;

  // Find all the pages whose favicons we should set, we want to set it for
  // all the pages in the redirect chain if it redirected.
  history::RedirectList redirects;
  GetCachedRecentRedirects(page_url, &redirects);

  bool mappings_changed = false;

  // Save page <-> favicon associations.
  for (history::RedirectList::const_iterator i(redirects.begin());
       i != redirects.end(); ++i) {
    mappings_changed |= SetFaviconMappingsForPage(*i, icon_type, icon_ids);
  }
  return mappings_changed;
}

bool HistoryBackend::SetFaviconMappingsForPage(
    const GURL& page_url,
    favicon_base::IconType icon_type,
    const std::vector<favicon_base::FaviconID>& icon_ids) {
  DCHECK_LE(icon_ids.size(), kMaxFaviconsPerPage);
  bool mappings_changed = false;

  // Two icon types are considered 'equivalent' if one of the icon types is
  // TOUCH_ICON and the other is TOUCH_PRECOMPOSED_ICON.
  //
  // Sets the icon mappings from |page_url| for |icon_type| to the favicons
  // with |icon_ids|. Mappings for |page_url| to favicons of type |icon_type|
  // whose FaviconID is not in |icon_ids| are removed. All icon mappings for
  // |page_url| to favicons of a type equivalent to |icon_type| are removed.
  // Remove any favicons which are orphaned as a result of the removal of the
  // icon mappings.

  std::vector<favicon_base::FaviconID> unmapped_icon_ids = icon_ids;

  std::vector<IconMapping> icon_mappings;
  thumbnail_db_->GetIconMappingsForPageURL(page_url, &icon_mappings);

  for (std::vector<IconMapping>::iterator m = icon_mappings.begin();
       m != icon_mappings.end(); ++m) {
    std::vector<favicon_base::FaviconID>::iterator icon_id_it = std::find(
        unmapped_icon_ids.begin(), unmapped_icon_ids.end(), m->icon_id);

    // If the icon mapping already exists, avoid removing it and adding it back.
    if (icon_id_it != unmapped_icon_ids.end()) {
      unmapped_icon_ids.erase(icon_id_it);
      continue;
    }

    if ((icon_type == favicon_base::TOUCH_ICON &&
         m->icon_type == favicon_base::TOUCH_PRECOMPOSED_ICON) ||
        (icon_type == favicon_base::TOUCH_PRECOMPOSED_ICON &&
         m->icon_type == favicon_base::TOUCH_ICON) ||
        (icon_type == m->icon_type)) {
      thumbnail_db_->DeleteIconMapping(m->mapping_id);

      // Removing the icon mapping may have orphaned the associated favicon so
      // we must recheck it. This is not super fast, but this case will get
      // triggered rarely, since normally a page will always map to the same
      // favicon IDs. It will mostly happen for favicons we import.
      if (!thumbnail_db_->HasMappingFor(m->icon_id))
        thumbnail_db_->DeleteFavicon(m->icon_id);
      mappings_changed = true;
    }
  }

  for (size_t i = 0; i < unmapped_icon_ids.size(); ++i) {
    thumbnail_db_->AddIconMapping(page_url, unmapped_icon_ids[i]);
    mappings_changed = true;
  }
  return mappings_changed;
}

void HistoryBackend::GetCachedRecentRedirects(
    const GURL& page_url,
    history::RedirectList* redirect_list) {
  RedirectCache::iterator iter = recent_redirects_.Get(page_url);
  if (iter != recent_redirects_.end()) {
    *redirect_list = iter->second;

    // The redirect chain should have the destination URL as the last item.
    DCHECK(!redirect_list->empty());
    DCHECK(redirect_list->back() == page_url);
  } else {
    // No known redirects, construct mock redirect chain containing |page_url|.
    redirect_list->push_back(page_url);
  }
}

void HistoryBackend::SendFaviconChangedNotificationForPageAndRedirects(
    const GURL& page_url) {
  history::RedirectList redirect_list;
  GetCachedRecentRedirects(page_url, &redirect_list);

  scoped_ptr<FaviconChangedDetails> changed_details(new FaviconChangedDetails);
  for (size_t i = 0; i < redirect_list.size(); ++i)
    changed_details->urls.insert(redirect_list[i]);

  BroadcastNotifications(chrome::NOTIFICATION_FAVICON_CHANGED,
                         changed_details.PassAs<HistoryDetails>());
}

void HistoryBackend::Commit() {
  if (!db_)
    return;

  // Note that a commit may not actually have been scheduled if a caller
  // explicitly calls this instead of using ScheduleCommit. Likewise, we
  // may reset the flag written by a pending commit. But this is OK! It
  // will merely cause extra commits (which is kind of the idea). We
  // could optimize more for this case (we may get two extra commits in
  // some cases) but it hasn't been important yet.
  CancelScheduledCommit();

  db_->CommitTransaction();
  DCHECK(db_->transaction_nesting() == 0) << "Somebody left a transaction open";
  db_->BeginTransaction();

  if (thumbnail_db_) {
    thumbnail_db_->CommitTransaction();
    DCHECK(thumbnail_db_->transaction_nesting() == 0) <<
        "Somebody left a transaction open";
    thumbnail_db_->BeginTransaction();
  }
}

void HistoryBackend::ScheduleCommit() {
  if (scheduled_commit_.get())
    return;
  scheduled_commit_ = new CommitLaterTask(this);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&CommitLaterTask::RunCommit, scheduled_commit_.get()),
      base::TimeDelta::FromSeconds(kCommitIntervalSeconds));
}

void HistoryBackend::CancelScheduledCommit() {
  if (scheduled_commit_.get()) {
    scheduled_commit_->Cancel();
    scheduled_commit_ = NULL;
  }
}

void HistoryBackend::ProcessDBTaskImpl() {
  if (!db_) {
    // db went away, release all the refs.
    STLDeleteContainerPointers(queued_history_db_tasks_.begin(),
                               queued_history_db_tasks_.end());
    queued_history_db_tasks_.clear();
    return;
  }

  // Remove any canceled tasks.
  while (!queued_history_db_tasks_.empty()) {
    QueuedHistoryDBTask* task = queued_history_db_tasks_.front();
    if (!task->is_canceled())
      break;

    delete task;
    queued_history_db_tasks_.pop_front();
  }
  if (queued_history_db_tasks_.empty())
    return;

  // Run the first task.
  scoped_ptr<QueuedHistoryDBTask> task(queued_history_db_tasks_.front());
  queued_history_db_tasks_.pop_front();
  if (task->Run(this, db_.get())) {
    // The task is done, notify the callback.
    task->DoneRun();
  } else {
    // The task wants to run some more. Schedule it at the end of the current
    // tasks, and process it after an invoke later.
    queued_history_db_tasks_.push_back(task.release());
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&HistoryBackend::ProcessDBTaskImpl, this));
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// Generic operations
//
////////////////////////////////////////////////////////////////////////////////

void HistoryBackend::DeleteURLs(const std::vector<GURL>& urls) {
  expirer_.DeleteURLs(urls);

  db_->GetStartDate(&first_recorded_time_);
  // Force a commit, if the user is deleting something for privacy reasons, we
  // want to get it on disk ASAP.
  Commit();
}

void HistoryBackend::DeleteURL(const GURL& url) {
  expirer_.DeleteURL(url);

  db_->GetStartDate(&first_recorded_time_);
  // Force a commit, if the user is deleting something for privacy reasons, we
  // want to get it on disk ASAP.
  Commit();
}

void HistoryBackend::ExpireHistoryBetween(
    const std::set<GURL>& restrict_urls,
    Time begin_time,
    Time end_time) {
  if (!db_)
    return;

  if (begin_time.is_null() && (end_time.is_null() || end_time.is_max()) &&
      restrict_urls.empty()) {
    // Special case deleting all history so it can be faster and to reduce the
    // possibility of an information leak.
    DeleteAllHistory();
  } else {
    // Clearing parts of history, have the expirer do the depend
    expirer_.ExpireHistoryBetween(restrict_urls, begin_time, end_time);

    // Force a commit, if the user is deleting something for privacy reasons,
    // we want to get it on disk ASAP.
    Commit();
  }

  if (begin_time <= first_recorded_time_)
    db_->GetStartDate(&first_recorded_time_);
}

void HistoryBackend::ExpireHistoryForTimes(
    const std::set<base::Time>& times,
    base::Time begin_time, base::Time end_time) {
  if (times.empty() || !db_)
    return;

  DCHECK(*times.begin() >= begin_time)
      << "Min time is before begin time: "
      << times.begin()->ToJsTime() << " v.s. " << begin_time.ToJsTime();
  DCHECK(*times.rbegin() < end_time)
      << "Max time is after end time: "
      << times.rbegin()->ToJsTime() << " v.s. " << end_time.ToJsTime();

  history::QueryOptions options;
  options.begin_time = begin_time;
  options.end_time = end_time;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  QueryResults results;
  QueryHistoryBasic(options, &results);

  // 1st pass: find URLs that are visited at one of |times|.
  std::set<GURL> urls;
  for (size_t i = 0; i < results.size(); ++i) {
    if (times.count(results[i].visit_time()) > 0)
      urls.insert(results[i].url());
  }
  if (urls.empty())
    return;

  // 2nd pass: collect all visit times of those URLs.
  std::vector<base::Time> times_to_expire;
  for (size_t i = 0; i < results.size(); ++i) {
    if (urls.count(results[i].url()))
      times_to_expire.push_back(results[i].visit_time());
  }

  // Put the times in reverse chronological order and remove
  // duplicates (for expirer_.ExpireHistoryForTimes()).
  std::sort(times_to_expire.begin(), times_to_expire.end(),
            std::greater<base::Time>());
  times_to_expire.erase(
      std::unique(times_to_expire.begin(), times_to_expire.end()),
      times_to_expire.end());

  // Expires by times and commit.
  DCHECK(!times_to_expire.empty());
  expirer_.ExpireHistoryForTimes(times_to_expire);
  Commit();

  DCHECK(times_to_expire.back() >= first_recorded_time_);
  // Update |first_recorded_time_| if we expired it.
  if (times_to_expire.back() == first_recorded_time_)
    db_->GetStartDate(&first_recorded_time_);
}

void HistoryBackend::ExpireHistory(
    const std::vector<history::ExpireHistoryArgs>& expire_list) {
  if (db_) {
    bool update_first_recorded_time = false;

    for (std::vector<history::ExpireHistoryArgs>::const_iterator it =
         expire_list.begin(); it != expire_list.end(); ++it) {
      expirer_.ExpireHistoryBetween(it->urls, it->begin_time, it->end_time);

      if (it->begin_time < first_recorded_time_)
        update_first_recorded_time = true;
    }
    Commit();

    // Update |first_recorded_time_| if any deletion might have affected it.
    if (update_first_recorded_time)
      db_->GetStartDate(&first_recorded_time_);
  }
}

void HistoryBackend::URLsNoLongerBookmarked(const std::set<GURL>& urls) {
  if (!db_)
    return;

  for (std::set<GURL>::const_iterator i = urls.begin(); i != urls.end(); ++i) {
    URLRow url_row;
    if (!db_->GetRowForURL(*i, &url_row))
      continue;  // The URL isn't in the db; nothing to do.

    VisitVector visits;
    db_->GetVisitsForURL(url_row.id(), &visits);

    if (visits.empty())
      expirer_.DeleteURL(*i);  // There are no more visits; nuke the URL.
  }
}

void HistoryBackend::DatabaseErrorCallback(int error, sql::Statement* stmt) {
  if (!scheduled_kill_db_ && sql::IsErrorCatastrophic(error)) {
    scheduled_kill_db_ = true;
    // Don't just do the close/delete here, as we are being called by |db| and
    // that seems dangerous.
    // TODO(shess): Consider changing KillHistoryDatabase() to use
    // RazeAndClose().  Then it can be cleared immediately.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&HistoryBackend::KillHistoryDatabase, this));
  }
}

void HistoryBackend::KillHistoryDatabase() {
  scheduled_kill_db_ = false;
  if (!db_)
    return;

  // Rollback transaction because Raze() cannot be called from within a
  // transaction.
  db_->RollbackTransaction();
  bool success = db_->Raze();
  UMA_HISTOGRAM_BOOLEAN("History.KillHistoryDatabaseResult", success);

#if defined(OS_ANDROID)
  // Release AndroidProviderBackend before other objects.
  android_provider_backend_.reset();
#endif

  // The expirer keeps tabs on the active databases. Tell it about the
  // databases which will be closed.
  expirer_.SetDatabases(NULL, NULL);

  // Reopen a new transaction for |db_| for the sake of CloseAllDatabases().
  db_->BeginTransaction();
  CloseAllDatabases();
}

void HistoryBackend::ProcessDBTask(
    scoped_ptr<HistoryDBTask> task,
    scoped_refptr<base::SingleThreadTaskRunner> origin_loop,
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled) {
  bool scheduled = !queued_history_db_tasks_.empty();
  queued_history_db_tasks_.push_back(
      new QueuedHistoryDBTask(task.Pass(), origin_loop, is_canceled));
  if (!scheduled)
    ProcessDBTaskImpl();
}

void HistoryBackend::BroadcastNotifications(
    int type,
    scoped_ptr<HistoryDetails> details) {
  // |delegate_| may be NULL if |this| is in the process of closing (closed by
  // HistoryService -> HistoryBackend::Closing().
  if (delegate_)
    delegate_->BroadcastNotifications(type, details.Pass());
}

void HistoryBackend::NotifySyncURLsModified(URLRows* rows) {
  if (typed_url_syncable_service_.get())
    typed_url_syncable_service_->OnUrlsModified(rows);
}

void HistoryBackend::NotifySyncURLsDeleted(bool all_history,
                                           bool expired,
                                           URLRows* rows) {
  if (typed_url_syncable_service_.get())
    typed_url_syncable_service_->OnUrlsDeleted(all_history, expired, rows);
}

// Deleting --------------------------------------------------------------------

void HistoryBackend::DeleteAllHistory() {
  // Our approach to deleting all history is:
  //  1. Copy the bookmarks and their dependencies to new tables with temporary
  //     names.
  //  2. Delete the original tables. Since tables can not share pages, we know
  //     that any data we don't want to keep is now in an unused page.
  //  3. Renaming the temporary tables to match the original.
  //  4. Vacuuming the database to delete the unused pages.
  //
  // Since we are likely to have very few bookmarks and their dependencies
  // compared to all history, this is also much faster than just deleting from
  // the original tables directly.

  // Get the bookmarked URLs.
  std::vector<URLAndTitle> starred_urls;
  HistoryClient* history_client = GetHistoryClient();
  if (history_client)
    history_client->GetBookmarks(&starred_urls);

  URLRows kept_urls;
  for (size_t i = 0; i < starred_urls.size(); i++) {
    URLRow row;
    if (!db_->GetRowForURL(starred_urls[i].url, &row))
      continue;

    // Clear the last visit time so when we write these rows they are "clean."
    row.set_last_visit(Time());
    row.set_visit_count(0);
    row.set_typed_count(0);
    kept_urls.push_back(row);
  }

  // Clear thumbnail and favicon history. The favicons for the given URLs will
  // be kept.
  if (!ClearAllThumbnailHistory(kept_urls)) {
    LOG(ERROR) << "Thumbnail history could not be cleared";
    // We continue in this error case. If the user wants to delete their
    // history, we should delete as much as we can.
  }

  // ClearAllMainHistory will change the IDs of the URLs in kept_urls.
  // Therefore, we clear the list afterwards to make sure nobody uses this
  // invalid data.
  if (!ClearAllMainHistory(kept_urls))
    LOG(ERROR) << "Main history could not be cleared";
  kept_urls.clear();

  db_->GetStartDate(&first_recorded_time_);

  // Send out the notification that history is cleared. The in-memory database
  // will pick this up and clear itself.
  scoped_ptr<URLsDeletedDetails> details(new URLsDeletedDetails);
  details->all_history = true;
  NotifySyncURLsDeleted(true, false, NULL);
  BroadcastNotifications(chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                         details.PassAs<HistoryDetails>());
}

bool HistoryBackend::ClearAllThumbnailHistory(const URLRows& kept_urls) {
  if (!thumbnail_db_) {
    // When we have no reference to the thumbnail database, maybe there was an
    // error opening it. In this case, we just try to blow it away to try to
    // fix the error if it exists. This may fail, in which case either the
    // file doesn't exist or there's no more we can do.
    sql::Connection::Delete(GetFaviconsFileName());

    // Older version of the database.
    sql::Connection::Delete(GetThumbnailFileName());
    return true;
  }

  // Urls to retain mappings for.
  std::vector<GURL> urls_to_keep;
  for (URLRows::const_iterator i = kept_urls.begin();
       i != kept_urls.end(); ++i) {
    urls_to_keep.push_back(i->url());
  }

  // Isolate from any long-running transaction.
  thumbnail_db_->CommitTransaction();
  thumbnail_db_->BeginTransaction();

  // TODO(shess): If this fails, perhaps the database should be razed
  // or deleted.
  if (!thumbnail_db_->RetainDataForPageUrls(urls_to_keep)) {
    thumbnail_db_->RollbackTransaction();
    thumbnail_db_->BeginTransaction();
    return false;
  }

#if defined(OS_ANDROID)
  // TODO (michaelbai): Add the unit test once AndroidProviderBackend is
  // avaliable in HistoryBackend.
  db_->ClearAndroidURLRows();
#endif

  // Vacuum to remove all the pages associated with the dropped tables. There
  // must be no transaction open on the table when we do this. We assume that
  // our long-running transaction is open, so we complete it and start it again.
  DCHECK(thumbnail_db_->transaction_nesting() == 1);
  thumbnail_db_->CommitTransaction();
  thumbnail_db_->Vacuum();
  thumbnail_db_->BeginTransaction();
  return true;
}

bool HistoryBackend::ClearAllMainHistory(const URLRows& kept_urls) {
  // Create the duplicate URL table. We will copy the kept URLs into this.
  if (!db_->CreateTemporaryURLTable())
    return false;

  // Insert the URLs into the temporary table.
  for (URLRows::const_iterator i = kept_urls.begin(); i != kept_urls.end();
       ++i) {
    db_->AddTemporaryURL(*i);
  }

  // Replace the original URL table with the temporary one.
  if (!db_->CommitTemporaryURLTable())
    return false;

  // Delete the old tables and recreate them empty.
  db_->RecreateAllTablesButURL();

  // Vacuum to reclaim the space from the dropped tables. This must be done
  // when there is no transaction open, and we assume that our long-running
  // transaction is currently open.
  db_->CommitTransaction();
  db_->Vacuum();
  db_->BeginTransaction();
  db_->GetStartDate(&first_recorded_time_);

  return true;
}

HistoryClient* HistoryBackend::GetHistoryClient() {
  if (history_client_)
    history_client_->BlockUntilBookmarksLoaded();
  return history_client_;
}

void HistoryBackend::NotifyVisitObservers(const VisitRow& visit) {
  BriefVisitInfo info;
  info.url_id = visit.url_id;
  info.time = visit.visit_time;
  info.transition = visit.transition;
  // If we don't have a delegate yet during setup or shutdown, we will drop
  // these notifications.
  if (delegate_)
    delegate_->NotifyVisitDBObserversOnAddVisit(info);
}

#if defined(OS_ANDROID)
void HistoryBackend::PopulateMostVisitedURLMap() {
  MostVisitedURLList most_visited_urls;
  QueryMostVisitedURLs(
      kPageVisitStatsMaxTopSites, kSegmentDataRetention, &most_visited_urls);

  DCHECK_LE(most_visited_urls.size(), kPageVisitStatsMaxTopSites);
  for (size_t i = 0; i < most_visited_urls.size(); ++i) {
    most_visited_urls_map_[most_visited_urls[i].url] = i;
    for (size_t j = 0; j < most_visited_urls[i].redirects.size(); ++j)
      most_visited_urls_map_[most_visited_urls[i].redirects[j]] = i;
  }
}

void HistoryBackend::RecordTopPageVisitStats(const GURL& url) {
  int rank = kPageVisitStatsMaxTopSites;
  std::map<GURL, int>::const_iterator it = most_visited_urls_map_.find(url);
  if (it != most_visited_urls_map_.end())
    rank = (*it).second;
  UMA_HISTOGRAM_ENUMERATION("History.TopSitesVisitsByRank",
                            rank, kPageVisitStatsMaxTopSites + 1);
}
#endif

}  // namespace history
