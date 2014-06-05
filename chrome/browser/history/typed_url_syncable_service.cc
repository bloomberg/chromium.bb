// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/typed_url_syncable_service.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "net/base/net_util.h"
#include "sync/protocol/sync.pb.h"
#include "sync/protocol/typed_url_specifics.pb.h"

namespace {

// The server backend can't handle arbitrarily large node sizes, so to keep
// the size under control we limit the visit array.
static const int kMaxTypedUrlVisits = 100;

// There's no limit on how many visits the history DB could have for a given
// typed URL, so we limit how many we fetch from the DB to avoid crashes due to
// running out of memory (http://crbug.com/89793). This value is different
// from kMaxTypedUrlVisits, as some of the visits fetched from the DB may be
// RELOAD visits, which will be stripped.
static const int kMaxVisitsToFetch = 1000;

// This is the threshold at which we start throttling sync updates for typed
// URLs - any URLs with a typed_count >= this threshold will be throttled.
static const int kTypedUrlVisitThrottleThreshold = 10;

// This is the multiple we use when throttling sync updates. If the multiple is
// N, we sync up every Nth update (i.e. when typed_count % N == 0).
static const int kTypedUrlVisitThrottleMultiple = 10;

}  // namespace

namespace history {

const char kTypedUrlTag[] = "google_chrome_typed_urls";

static bool CheckVisitOrdering(const VisitVector& visits) {
  int64 previous_visit_time = 0;
  for (VisitVector::const_iterator visit = visits.begin();
       visit != visits.end(); ++visit) {
    if (visit != visits.begin()) {
      // We allow duplicate visits here - they shouldn't really be allowed, but
      // they still seem to show up sometimes and we haven't figured out the
      // source, so we just log an error instead of failing an assertion.
      // (http://crbug.com/91473).
      if (previous_visit_time == visit->visit_time.ToInternalValue())
        DVLOG(1) << "Duplicate visit time encountered";
      else if (previous_visit_time > visit->visit_time.ToInternalValue())
        return false;
    }

    previous_visit_time = visit->visit_time.ToInternalValue();
  }
  return true;
}

TypedUrlSyncableService::TypedUrlSyncableService(
    HistoryBackend* history_backend)
    : history_backend_(history_backend),
      processing_syncer_changes_(false),
      expected_loop_(base::MessageLoop::current()) {
  DCHECK(history_backend_);
  DCHECK(expected_loop_ == base::MessageLoop::current());
}

TypedUrlSyncableService::~TypedUrlSyncableService() {
  DCHECK(expected_loop_ == base::MessageLoop::current());
}

syncer::SyncMergeResult TypedUrlSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(expected_loop_ == base::MessageLoop::current());
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(error_handler.get());
  DCHECK_EQ(type, syncer::TYPED_URLS);

  syncer::SyncMergeResult merge_result(type);
  sync_processor_ = sync_processor.Pass();
  sync_error_handler_ = error_handler.Pass();

  // TODO(mgist): Add implementation

  return merge_result;
}

void TypedUrlSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK(expected_loop_ == base::MessageLoop::current());
  DCHECK_EQ(type, syncer::TYPED_URLS);

  sync_processor_.reset();
  sync_error_handler_.reset();
}

syncer::SyncDataList TypedUrlSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK(expected_loop_ == base::MessageLoop::current());
  syncer::SyncDataList list;

  // TODO(mgist): Add implementation

  return list;
}

syncer::SyncError TypedUrlSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK(expected_loop_ == base::MessageLoop::current());

  // TODO(mgist): Add implementation

  return syncer::SyncError(FROM_HERE,
                           syncer::SyncError::DATATYPE_ERROR,
                           "Typed url syncable service is not implemented.",
                           syncer::TYPED_URLS);
}

void TypedUrlSyncableService::OnUrlsModified(URLRows* changed_urls) {
  DCHECK(expected_loop_ == base::MessageLoop::current());
  DCHECK(changed_urls);

  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.
  if (!sync_processor_.get())
    return;  // Sync processor not yet initialized, don't sync.

  // Create SyncChangeList.
  syncer::SyncChangeList changes;

  for (URLRows::iterator url = changed_urls->begin();
       url != changed_urls->end(); ++url) {
    // Only care if the modified URL is typed.
    if (url->typed_count() > 0) {
      // If there were any errors updating the sync node, just ignore them and
      // continue on to process the next URL.
      CreateOrUpdateSyncNode(*url, &changes);
    }
  }

  // Send SyncChangeList to server if there are any changes.
  if (changes.size() > 0)
    sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void TypedUrlSyncableService::OnUrlVisited(content::PageTransition transition,
                                           URLRow* row) {
  DCHECK(expected_loop_ == base::MessageLoop::current());
  DCHECK(row);

  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.
  if (!sync_processor_.get())
    return;  // Sync processor not yet initialized, don't sync.
  if (!ShouldSyncVisit(transition, row))
    return;

  // Create SyncChangeList.
  syncer::SyncChangeList changes;

  CreateOrUpdateSyncNode(*row, &changes);

  // Send SyncChangeList to server if there are any changes.
  if (changes.size() > 0)
    sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void TypedUrlSyncableService::OnUrlsDeleted(bool all_history,
                                            bool expired,
                                            URLRows* rows) {
  DCHECK(expected_loop_ == base::MessageLoop::current());

  if (processing_syncer_changes_)
    return;  // These are changes originating from us, ignore.
  if (!sync_processor_.get())
    return;  // Sync processor not yet initialized, don't sync.

  // Ignore URLs expired due to old age (we don't want to sync them as deletions
  // to avoid extra traffic up to the server, and also to make sure that a
  // client with a bad clock setting won't go on an expiration rampage and
  // delete all history from every client). The server will gracefully age out
  // the sync DB entries when they've been idle for long enough.
  if (expired)
    return;

  // Create SyncChangeList.
  syncer::SyncChangeList changes;

  if (all_history) {
    // Delete all synced typed urls.
    for (std::set<GURL>::const_iterator url = synced_typed_urls_.begin();
         url != synced_typed_urls_.end(); ++url) {
      VisitVector visits;
      URLRow row(*url);
      AddTypedUrlToChangeList(syncer::SyncChange::ACTION_DELETE,
                              row, visits, url->spec(), &changes);
    }
    // Clear cache of server state.
    synced_typed_urls_.clear();
  } else {
    DCHECK(rows);
    // Delete rows.
    for (URLRows::const_iterator row = rows->begin();
         row != rows->end(); ++row) {
      // Add specifics to change list for all synced urls that were deleted.
      if (synced_typed_urls_.find(row->url()) != synced_typed_urls_.end()) {
        VisitVector visits;
        AddTypedUrlToChangeList(syncer::SyncChange::ACTION_DELETE,
                                *row, visits, row->url().spec(), &changes);
        // Delete typed url from cache.
        synced_typed_urls_.erase(row->url());
      }
    }
  }

  // Send SyncChangeList to server if there are any changes.
  if (changes.size() > 0)
    sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

bool TypedUrlSyncableService::ShouldIgnoreUrl(const GURL& url) {
  // Ignore empty URLs. Not sure how this can happen (maybe import from other
  // busted browsers, or misuse of the history API, or just plain bugs) but we
  // can't deal with them.
  if (url.spec().empty())
    return true;

  // Ignore local file URLs.
  if (url.SchemeIsFile())
    return true;

  // Ignore localhost URLs.
  if (net::IsLocalhost(url.host()))
    return true;

  return false;
}

bool TypedUrlSyncableService::ShouldSyncVisit(
    content::PageTransition page_transition,
    URLRow* row) {
  if (!row)
    return false;
  int typed_count = row->typed_count();
  content::PageTransition transition = static_cast<content::PageTransition>(
      page_transition & content::PAGE_TRANSITION_CORE_MASK);

  // Just use an ad-hoc criteria to determine whether to ignore this
  // notification. For most users, the distribution of visits is roughly a bell
  // curve with a long tail - there are lots of URLs with < 5 visits so we want
  // to make sure we sync up every visit to ensure the proper ordering of
  // suggestions. But there are relatively few URLs with > 10 visits, and those
  // tend to be more broadly distributed such that there's no need to sync up
  // every visit to preserve their relative ordering.
  return (transition == content::PAGE_TRANSITION_TYPED &&
          typed_count > 0 &&
          (typed_count < kTypedUrlVisitThrottleThreshold ||
           (typed_count % kTypedUrlVisitThrottleMultiple) == 0));
}

bool TypedUrlSyncableService::CreateOrUpdateSyncNode(
    URLRow url,
    syncer::SyncChangeList* changes) {
  DCHECK_GT(url.typed_count(), 0);

  if (ShouldIgnoreUrl(url.url()))
    return true;

  // Get the visits for this node.
  VisitVector visit_vector;
  if (!FixupURLAndGetVisits(&url, &visit_vector)) {
    DLOG(ERROR) << "Could not load visits for url: " << url.url();
    return false;
  }
  DCHECK(!visit_vector.empty());

  std::string title = url.url().spec();
  syncer::SyncChange::SyncChangeType change_type;

  // If server already has URL, then send a sync update, else add it.
  change_type =
      (synced_typed_urls_.find(url.url()) != synced_typed_urls_.end()) ?
      syncer::SyncChange::ACTION_UPDATE :
      syncer::SyncChange::ACTION_ADD;

  // Ensure cache of server state is up to date.
  synced_typed_urls_.insert(url.url());

  AddTypedUrlToChangeList(change_type, url, visit_vector, title, changes);

  return true;
}

void TypedUrlSyncableService::AddTypedUrlToChangeList(
    syncer::SyncChange::SyncChangeType change_type,
    const URLRow& row,
    const VisitVector& visits,
    std::string title,
    syncer::SyncChangeList* change_list) {
  sync_pb::EntitySpecifics entity_specifics;
  sync_pb::TypedUrlSpecifics* typed_url = entity_specifics.mutable_typed_url();

  if (change_type == syncer::SyncChange::ACTION_DELETE) {
    typed_url->set_url(row.url().spec());
  } else {
    WriteToTypedUrlSpecifics(row, visits, typed_url);
  }

  change_list->push_back(
      syncer::SyncChange(FROM_HERE, change_type,
                         syncer::SyncData::CreateLocalData(
                             kTypedUrlTag, title, entity_specifics)));
}

void TypedUrlSyncableService::WriteToTypedUrlSpecifics(
    const URLRow& url,
    const VisitVector& visits,
    sync_pb::TypedUrlSpecifics* typed_url) {

  DCHECK(!url.last_visit().is_null());
  DCHECK(!visits.empty());
  DCHECK_EQ(url.last_visit().ToInternalValue(),
            visits.back().visit_time.ToInternalValue());

  typed_url->set_url(url.url().spec());
  typed_url->set_title(base::UTF16ToUTF8(url.title()));
  typed_url->set_hidden(url.hidden());

  DCHECK(CheckVisitOrdering(visits));

  bool only_typed = false;
  int skip_count = 0;

  if (visits.size() > static_cast<size_t>(kMaxTypedUrlVisits)) {
    int typed_count = 0;
    int total = 0;
    // Walk the passed-in visit vector and count the # of typed visits.
    for (VisitVector::const_iterator visit = visits.begin();
         visit != visits.end(); ++visit) {
      content::PageTransition transition = content::PageTransitionFromInt(
          visit->transition & content::PAGE_TRANSITION_CORE_MASK);
      // We ignore reload visits.
      if (transition == content::PAGE_TRANSITION_RELOAD)
        continue;
      ++total;
      if (transition == content::PAGE_TRANSITION_TYPED)
        ++typed_count;
    }
    // We should have at least one typed visit. This can sometimes happen if
    // the history DB has an inaccurate count for some reason (there's been
    // bugs in the history code in the past which has left users in the wild
    // with incorrect counts - http://crbug.com/84258).
    DCHECK(typed_count > 0);

    if (typed_count > kMaxTypedUrlVisits) {
      only_typed = true;
      skip_count = typed_count - kMaxTypedUrlVisits;
    } else if (total > kMaxTypedUrlVisits) {
      skip_count = total - kMaxTypedUrlVisits;
    }
  }

  for (VisitVector::const_iterator visit = visits.begin();
       visit != visits.end(); ++visit) {
    content::PageTransition transition = content::PageTransitionFromInt(
        visit->transition & content::PAGE_TRANSITION_CORE_MASK);
    // Skip reload visits.
    if (transition == content::PAGE_TRANSITION_RELOAD)
      continue;

    // If we only have room for typed visits, then only add typed visits.
    if (only_typed && transition != content::PAGE_TRANSITION_TYPED)
      continue;

    if (skip_count > 0) {
      // We have too many entries to fit, so we need to skip the oldest ones.
      // Only skip typed URLs if there are too many typed URLs to fit.
      if (only_typed || transition != content::PAGE_TRANSITION_TYPED) {
        --skip_count;
        continue;
      }
    }
    typed_url->add_visits(visit->visit_time.ToInternalValue());
    typed_url->add_visit_transitions(visit->transition);
  }
  DCHECK_EQ(skip_count, 0);

  if (typed_url->visits_size() == 0) {
    // If we get here, it's because we don't actually have any TYPED visits
    // even though the visit's typed_count > 0 (corrupted typed_count). So
    // let's go ahead and add a RELOAD visit at the most recent visit since
    // it's not legal to have an empty visit array (yet another workaround
    // for http://crbug.com/84258).
    typed_url->add_visits(url.last_visit().ToInternalValue());
    typed_url->add_visit_transitions(content::PAGE_TRANSITION_RELOAD);
  }
  CHECK_GT(typed_url->visits_size(), 0);
  CHECK_LE(typed_url->visits_size(), kMaxTypedUrlVisits);
  CHECK_EQ(typed_url->visits_size(), typed_url->visit_transitions_size());
}

bool TypedUrlSyncableService::FixupURLAndGetVisits(
    URLRow* url,
    VisitVector* visits) {
  ++num_db_accesses_;
  CHECK(history_backend_);
  if (!history_backend_->GetMostRecentVisitsForURL(
          url->id(), kMaxVisitsToFetch, visits)) {
    ++num_db_errors_;
    return false;
  }

  // Sometimes (due to a bug elsewhere in the history or sync code, or due to
  // a crash between adding a URL to the history database and updating the
  // visit DB) the visit vector for a URL can be empty. If this happens, just
  // create a new visit whose timestamp is the same as the last_visit time.
  // This is a workaround for http://crbug.com/84258.
  if (visits->empty()) {
    DVLOG(1) << "Found empty visits for URL: " << url->url();
    VisitRow visit(
        url->id(), url->last_visit(), 0, content::PAGE_TRANSITION_TYPED, 0);
    visits->push_back(visit);
  }

  // GetMostRecentVisitsForURL() returns the data in the opposite order that
  // we need it, so reverse it.
  std::reverse(visits->begin(), visits->end());

  // Sometimes, the last_visit field in the URL doesn't match the timestamp of
  // the last visit in our visit array (they come from different tables, so
  // crashes/bugs can cause them to mismatch), so just set it here.
  url->set_last_visit(visits->back().visit_time);
  DCHECK(CheckVisitOrdering(*visits));
  return true;
}

}  // namespace history
