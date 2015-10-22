// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_change_processor.h"

#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/typed_url_model_associator.h"
#include "content/public/browser/browser_thread.h"
#include "sync/internal_api/public/change_record.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/typed_url_specifics.pb.h"
#include "sync/syncable/entry.h"  // TODO(tim): Investigating bug 121587.

using content::BrowserThread;

namespace browser_sync {

// This is the threshold at which we start throttling sync updates for typed
// URLs - any URLs with a typed_count >= this threshold will be throttled.
static const int kTypedUrlVisitThrottleThreshold = 10;

// This is the multiple we use when throttling sync updates. If the multiple is
// N, we sync up every Nth update (i.e. when typed_count % N == 0).
static const int kTypedUrlVisitThrottleMultiple = 10;

TypedUrlChangeProcessor::TypedUrlChangeProcessor(
    Profile* profile,
    TypedUrlModelAssociator* model_associator,
    history::HistoryBackend* history_backend,
    sync_driver::DataTypeErrorHandler* error_handler)
    : sync_driver::ChangeProcessor(error_handler),
      profile_(profile),
      model_associator_(model_associator),
      history_backend_(history_backend),
      backend_loop_(base::MessageLoop::current()),
      disconnected_(false),
      history_backend_observer_(this) {
  DCHECK(model_associator);
  DCHECK(history_backend);
  DCHECK(error_handler);
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
}

TypedUrlChangeProcessor::~TypedUrlChangeProcessor() {
  DCHECK(backend_loop_ == base::MessageLoop::current());
  DCHECK(history_backend_);
  history_backend_->RemoveObserver(this);
}

void TypedUrlChangeProcessor::OnURLVisited(
    history::HistoryBackend* history_backend,
    ui::PageTransition transition,
    const history::URLRow& row,
    const history::RedirectList& redirects,
    base::Time visit_time) {
  DCHECK(backend_loop_ == base::MessageLoop::current());

  base::AutoLock al(disconnect_lock_);
  if (disconnected_)
    return;

  DVLOG(1) << "Observed typed_url change.";
  if (ShouldSyncVisit(row.typed_count(), transition)) {
    VisitsToSync visits_to_sync;
    if (FixupURLAndGetVisitsToSync(row, &visits_to_sync)) {
      syncer::WriteTransaction trans(FROM_HERE, share_handle());
      CreateOrUpdateSyncNode(&trans, visits_to_sync.row, visits_to_sync.visits);
    }
  }
  UMA_HISTOGRAM_PERCENTAGE("Sync.TypedUrlChangeProcessorErrors",
                           model_associator_->GetErrorPercentage());
}

void TypedUrlChangeProcessor::OnURLsModified(
    history::HistoryBackend* history_backend,
    const history::URLRows& changed_urls) {
  DCHECK(backend_loop_ == base::MessageLoop::current());

  base::AutoLock al(disconnect_lock_);
  if (disconnected_)
    return;

  std::vector<VisitsToSync> visits_to_sync_vector;

  DVLOG(1) << "Observed typed_url change.";
  for (const auto& row : changed_urls) {
    if (row.typed_count() >= 0) {
      VisitsToSync visits_to_sync;
      if (FixupURLAndGetVisitsToSync(row, &visits_to_sync)) {
        visits_to_sync_vector.push_back(visits_to_sync);
      }
    }
  }

  if (!visits_to_sync_vector.empty()) {
    syncer::WriteTransaction trans(FROM_HERE, share_handle());
    for (const auto& visits_to_sync : visits_to_sync_vector) {
      // If there were any errors updating the sync node, just ignore them and
      // continue on to process the next URL.
      CreateOrUpdateSyncNode(&trans, visits_to_sync.row, visits_to_sync.visits);
    }
  }

  UMA_HISTOGRAM_PERCENTAGE("Sync.TypedUrlChangeProcessorErrors",
                           model_associator_->GetErrorPercentage());
}

void TypedUrlChangeProcessor::OnURLsDeleted(
    history::HistoryBackend* history_backend,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  DCHECK(backend_loop_ == base::MessageLoop::current());

  base::AutoLock al(disconnect_lock_);
  if (disconnected_)
    return;

  DVLOG(1) << "Observed typed_url change.";

  syncer::WriteTransaction trans(FROM_HERE, share_handle());

  // Ignore archivals (we don't want to sync them as deletions, to avoid
  // extra traffic up to the server, and also to make sure that a client with
  // a bad clock setting won't go on an archival rampage and delete all
  // history from every client). The server will gracefully age out the sync DB
  // entries when they've been idle for long enough.
  if (expired)
    return;

  if (all_history) {
    if (!model_associator_->DeleteAllNodes(&trans)) {
      syncer::SyncError error(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                              "Failed to delete local nodes.",
                              syncer::TYPED_URLS);
      error_handler()->OnSingleDataTypeUnrecoverableError(error);
      return;
    }
  } else {
    for (const auto& row : deleted_rows) {
      syncer::WriteNode sync_node(&trans);
      // The deleted URL could have been non-typed, so it might not be found
      // in the sync DB.
      if (sync_node.InitByClientTagLookup(syncer::TYPED_URLS,
                                          row.url().spec()) ==
          syncer::BaseNode::INIT_OK) {
        sync_node.Tombstone();
      }
    }
  }
  UMA_HISTOGRAM_PERCENTAGE("Sync.TypedUrlChangeProcessorErrors",
                           model_associator_->GetErrorPercentage());
}

TypedUrlChangeProcessor::VisitsToSync::VisitsToSync() {}

TypedUrlChangeProcessor::VisitsToSync::~VisitsToSync() {}

bool TypedUrlChangeProcessor::FixupURLAndGetVisitsToSync(
    const history::URLRow& url,
    VisitsToSync* visits_to_sync) {
  DCHECK_GE(url.typed_count(), 0);

  // Get the visits for this node.
  visits_to_sync->row = url;
  if (!model_associator_->FixupURLAndGetVisits(&visits_to_sync->row,
                                               &visits_to_sync->visits)) {
    DLOG(ERROR) << "Could not load visits for url: " << url.url();
    return false;
  }

  if (std::find_if(visits_to_sync->visits.begin(), visits_to_sync->visits.end(),
                   [](const history::VisitRow& visit) {
                     return ui::PageTransitionCoreTypeIs(
                         visit.transition, ui::PAGE_TRANSITION_TYPED);
                   }) == visits_to_sync->visits.end()) {
    // This URL has no TYPED visits, don't sync it.
    return false;
  }

  if (model_associator_->ShouldIgnoreUrl(visits_to_sync->row.url()))
    return false;

  return true;
}

void TypedUrlChangeProcessor::CreateOrUpdateSyncNode(
    syncer::WriteTransaction* trans,
    const history::URLRow& url,
    const history::VisitVector& visit_vector) {
  DCHECK_GE(url.typed_count(), 0);
  DCHECK(!visit_vector.empty());

  std::string tag = url.url().spec();
  syncer::WriteNode update_node(trans);
  syncer::BaseNode::InitByLookupResult result =
      update_node.InitByClientTagLookup(syncer::TYPED_URLS, tag);
  if (result == syncer::BaseNode::INIT_OK) {
    model_associator_->WriteToSyncNode(url, visit_vector, &update_node);
  } else if (result == syncer::BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Failed to decrypt.",
                            syncer::TYPED_URLS);
    error_handler()->OnSingleDataTypeUnrecoverableError(error);
    return;
  } else {
    syncer::WriteNode create_node(trans);
    syncer::WriteNode::InitUniqueByCreationResult result =
        create_node.InitUniqueByCreation(syncer::TYPED_URLS, tag);
    if (result != syncer::WriteNode::INIT_SUCCESS) {

      syncer::SyncError error(FROM_HERE,
                              syncer::SyncError::DATATYPE_ERROR,
                              "Failed to create sync node",
                              syncer::TYPED_URLS);
      error_handler()->OnSingleDataTypeUnrecoverableError(error);
      return;
    }

    create_node.SetTitle(tag);
    model_associator_->WriteToSyncNode(url, visit_vector, &create_node);
  }
}

bool TypedUrlChangeProcessor::ShouldSyncVisit(int typed_count,
                                              ui::PageTransition transition) {
  // Just use an ad-hoc criteria to determine whether to ignore this
  // notification. For most users, the distribution of visits is roughly a bell
  // curve with a long tail - there are lots of URLs with < 5 visits so we want
  // to make sure we sync up every visit to ensure the proper ordering of
  // suggestions. But there are relatively few URLs with > 10 visits, and those
  // tend to be more broadly distributed such that there's no need to sync up
  // every visit to preserve their relative ordering.
  return (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) &&
          typed_count >= 0 &&
          (typed_count < kTypedUrlVisitThrottleThreshold ||
           (typed_count % kTypedUrlVisitThrottleMultiple) == 0));
}

void TypedUrlChangeProcessor::ApplyChangesFromSyncModel(
    const syncer::BaseTransaction* trans,
    int64 model_version,
    const syncer::ImmutableChangeRecordList& changes) {
  DCHECK(backend_loop_ == base::MessageLoop::current());

  base::AutoLock al(disconnect_lock_);
  if (disconnected_)
    return;

  syncer::ReadNode typed_url_root(trans);
  if (typed_url_root.InitTypeRoot(syncer::TYPED_URLS) !=
          syncer::BaseNode::INIT_OK) {
    syncer::SyncError error(FROM_HERE,
                            syncer::SyncError::DATATYPE_ERROR,
                            "Failed to init type root.",
                            syncer::TYPED_URLS);
    error_handler()->OnSingleDataTypeUnrecoverableError(error);
    return;
  }

  DCHECK(pending_new_urls_.empty() && pending_new_visits_.empty() &&
         pending_deleted_visits_.empty() && pending_updated_urls_.empty() &&
         pending_deleted_urls_.empty());

  for (syncer::ChangeRecordList::const_iterator it =
           changes.Get().begin(); it != changes.Get().end(); ++it) {
    if (syncer::ChangeRecord::ACTION_DELETE ==
        it->action) {
      DCHECK(it->specifics.has_typed_url()) <<
          "Typed URL delete change does not have necessary specifics.";
      GURL url(it->specifics.typed_url().url());
      pending_deleted_urls_.push_back(url);
      continue;
    }

    syncer::ReadNode sync_node(trans);
    if (sync_node.InitByIdLookup(it->id) != syncer::BaseNode::INIT_OK) {
      syncer::SyncError error(FROM_HERE,
                              syncer::SyncError::DATATYPE_ERROR,
                              "Failed to init sync node.",
                              syncer::TYPED_URLS);
      error_handler()->OnSingleDataTypeUnrecoverableError(error);
      return;
    }

    DCHECK(syncer::TYPED_URLS == sync_node.GetModelType());

    const sync_pb::TypedUrlSpecifics& typed_url(
        sync_node.GetTypedUrlSpecifics());
    DCHECK(typed_url.visits_size());

    if (model_associator_->ShouldIgnoreUrl(GURL(typed_url.url())))
      continue;

    sync_pb::TypedUrlSpecifics filtered_url =
        model_associator_->FilterExpiredVisits(typed_url);
    if (!filtered_url.visits_size()) {
      continue;
    }

    model_associator_->UpdateFromSyncDB(
        filtered_url, &pending_new_visits_, &pending_deleted_visits_,
        &pending_updated_urls_, &pending_new_urls_);
  }
}

void TypedUrlChangeProcessor::CommitChangesFromSyncModel() {
  DCHECK(backend_loop_ == base::MessageLoop::current());

  base::AutoLock al(disconnect_lock_);
  if (disconnected_)
    return;

  // Make sure we stop listening for changes while we're modifying the backend,
  // so we don't try to re-apply these changes to the sync DB.
  ScopedStopObserving<TypedUrlChangeProcessor> stop_observing(this);
  if (!pending_deleted_urls_.empty())
    history_backend_->DeleteURLs(pending_deleted_urls_);

  model_associator_->WriteToHistoryBackend(&pending_new_urls_,
                                           &pending_updated_urls_,
                                           &pending_new_visits_,
                                           &pending_deleted_visits_);

  pending_new_urls_.clear();
  pending_updated_urls_.clear();
  pending_new_visits_.clear();
  pending_deleted_visits_.clear();
  pending_deleted_urls_.clear();
  UMA_HISTOGRAM_PERCENTAGE("Sync.TypedUrlChangeProcessorErrors",
                           model_associator_->GetErrorPercentage());
}

void TypedUrlChangeProcessor::Disconnect() {
  base::AutoLock al(disconnect_lock_);
  disconnected_ = true;
}

void TypedUrlChangeProcessor::StartImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(history_backend_);
  DCHECK(backend_loop_);
  backend_loop_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&TypedUrlChangeProcessor::StartObserving,
                            base::Unretained(this)));
}

void TypedUrlChangeProcessor::StartObserving() {
  DCHECK(backend_loop_ == base::MessageLoop::current());
  DCHECK(history_backend_);
  DCHECK(profile_);
  history_backend_observer_.Add(history_backend_);
}

void TypedUrlChangeProcessor::StopObserving() {
  DCHECK(backend_loop_ == base::MessageLoop::current());
  DCHECK(history_backend_);
  DCHECK(profile_);
  history_backend_observer_.RemoveAll();
}

}  // namespace browser_sync
