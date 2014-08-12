// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_change_processor.h"

#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
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
      disconnected_(false) {
  DCHECK(model_associator);
  DCHECK(history_backend);
  DCHECK(error_handler);
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  // When running in unit tests, there is already a NotificationService object.
  // Since only one can exist at a time per thread, check first.
  if (!content::NotificationService::current())
    notification_service_.reset(content::NotificationService::Create());
}

TypedUrlChangeProcessor::~TypedUrlChangeProcessor() {
  DCHECK(backend_loop_ == base::MessageLoop::current());
}

void TypedUrlChangeProcessor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(backend_loop_ == base::MessageLoop::current());

  base::AutoLock al(disconnect_lock_);
  if (disconnected_)
    return;

  DVLOG(1) << "Observed typed_url change.";
  if (type == chrome::NOTIFICATION_HISTORY_URLS_MODIFIED) {
    HandleURLsModified(
        content::Details<history::URLsModifiedDetails>(details).ptr());
  } else if (type == chrome::NOTIFICATION_HISTORY_URLS_DELETED) {
    HandleURLsDeleted(
        content::Details<history::URLsDeletedDetails>(details).ptr());
  } else {
    DCHECK_EQ(chrome::NOTIFICATION_HISTORY_URL_VISITED, type);
    HandleURLsVisited(
        content::Details<history::URLVisitedDetails>(details).ptr());
  }
  UMA_HISTOGRAM_PERCENTAGE("Sync.TypedUrlChangeProcessorErrors",
                           model_associator_->GetErrorPercentage());
}

void TypedUrlChangeProcessor::HandleURLsModified(
    history::URLsModifiedDetails* details) {

  syncer::WriteTransaction trans(FROM_HERE, share_handle());
  for (history::URLRows::iterator url = details->changed_urls.begin();
       url != details->changed_urls.end(); ++url) {
    if (url->typed_count() > 0) {
      // If there were any errors updating the sync node, just ignore them and
      // continue on to process the next URL.
      CreateOrUpdateSyncNode(*url, &trans);
    }
  }
}

bool TypedUrlChangeProcessor::CreateOrUpdateSyncNode(
    history::URLRow url, syncer::WriteTransaction* trans) {
  DCHECK_GT(url.typed_count(), 0);
  // Get the visits for this node.
  history::VisitVector visit_vector;
  if (!model_associator_->FixupURLAndGetVisits(&url, &visit_vector)) {
    DLOG(ERROR) << "Could not load visits for url: " << url.url();
    return false;
  }

  syncer::ReadNode typed_url_root(trans);
  if (typed_url_root.InitTypeRoot(syncer::TYPED_URLS) !=
          syncer::BaseNode::INIT_OK) {
    error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
        "Server did not create the top-level typed_url node. We "
         "might be running against an out-of-date server.");
    return false;
  }

  if (model_associator_->ShouldIgnoreUrl(url.url()))
    return true;

  DCHECK(!visit_vector.empty());
  std::string tag = url.url().spec();
  syncer::WriteNode update_node(trans);
  syncer::BaseNode::InitByLookupResult result =
      update_node.InitByClientTagLookup(syncer::TYPED_URLS, tag);
  if (result == syncer::BaseNode::INIT_OK) {
    model_associator_->WriteToSyncNode(url, visit_vector, &update_node);
  } else if (result == syncer::BaseNode::INIT_FAILED_DECRYPT_IF_NECESSARY) {
    // TODO(tim): Investigating bug 121587.
    syncer::Cryptographer* crypto = trans->GetCryptographer();
    syncer::ModelTypeSet encrypted_types(trans->GetEncryptedTypes());
    const sync_pb::EntitySpecifics& specifics =
        update_node.GetEntry()->GetSpecifics();
    CHECK(specifics.has_encrypted());
    const bool can_decrypt = crypto->CanDecrypt(specifics.encrypted());
    const bool agreement = encrypted_types.Has(syncer::TYPED_URLS);
    if (!agreement && !can_decrypt) {
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
          "Could not InitByIdLookup in CreateOrUpdateSyncNode, "
          " Cryptographer thinks typed urls not encrypted, and CanDecrypt"
          " failed.");
      LOG(ERROR) << "Case 1.";
    } else if (agreement && can_decrypt) {
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
          "Could not InitByIdLookup on CreateOrUpdateSyncNode, "
          " Cryptographer thinks typed urls are encrypted, and CanDecrypt"
          " succeeded (?!), but DecryptIfNecessary failed.");
      LOG(ERROR) << "Case 2.";
    } else if (agreement) {
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
          "Could not InitByIdLookup on CreateOrUpdateSyncNode, "
          " Cryptographer thinks typed urls are encrypted, but CanDecrypt"
          " failed.");
      LOG(ERROR) << "Case 3.";
    } else {
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
          "Could not InitByIdLookup on CreateOrUpdateSyncNode, "
          " Cryptographer thinks typed urls not encrypted, but CanDecrypt"
          " succeeded (super weird, btw)");
      LOG(ERROR) << "Case 4.";
    }
  } else {
    syncer::WriteNode create_node(trans);
    syncer::WriteNode::InitUniqueByCreationResult result =
        create_node.InitUniqueByCreation(syncer::TYPED_URLS,
                                         typed_url_root, tag);
    if (result != syncer::WriteNode::INIT_SUCCESS) {
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
          "Failed to create typed_url sync node.");
      return false;
    }

    create_node.SetTitle(tag);
    model_associator_->WriteToSyncNode(url, visit_vector, &create_node);
  }
  return true;
}

void TypedUrlChangeProcessor::HandleURLsDeleted(
    history::URLsDeletedDetails* details) {
  syncer::WriteTransaction trans(FROM_HERE, share_handle());

  // Ignore archivals (we don't want to sync them as deletions, to avoid
  // extra traffic up to the server, and also to make sure that a client with
  // a bad clock setting won't go on an archival rampage and delete all
  // history from every client). The server will gracefully age out the sync DB
  // entries when they've been idle for long enough.
  if (details->expired)
    return;

  if (details->all_history) {
    if (!model_associator_->DeleteAllNodes(&trans)) {
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
          std::string());
      return;
    }
  } else {
    for (history::URLRows::const_iterator row = details->rows.begin();
         row != details->rows.end(); ++row) {
      syncer::WriteNode sync_node(&trans);
      // The deleted URL could have been non-typed, so it might not be found
      // in the sync DB.
      if (sync_node.InitByClientTagLookup(syncer::TYPED_URLS,
                                          row->url().spec()) ==
              syncer::BaseNode::INIT_OK) {
        sync_node.Tombstone();
      }
    }
  }
}

void TypedUrlChangeProcessor::HandleURLsVisited(
    history::URLVisitedDetails* details) {
  if (!ShouldSyncVisit(details))
    return;

  syncer::WriteTransaction trans(FROM_HERE, share_handle());
  CreateOrUpdateSyncNode(details->row, &trans);
}

bool TypedUrlChangeProcessor::ShouldSyncVisit(
    history::URLVisitedDetails* details) {
  int typed_count = details->row.typed_count();
  content::PageTransition transition = static_cast<content::PageTransition>(
      details->transition & content::PAGE_TRANSITION_CORE_MASK);

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
    error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
        "TypedUrl root node lookup failed.");
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
      error_handler()->OnSingleDatatypeUnrecoverableError(FROM_HERE,
          "TypedUrl node lookup failed.");
      return;
    }

    // Check that the changed node is a child of the typed_urls folder.
    DCHECK(typed_url_root.GetId() == sync_node.GetParentId());
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(history_backend_);
  DCHECK(backend_loop_);
  backend_loop_->PostTask(FROM_HERE,
                          base::Bind(&TypedUrlChangeProcessor::StartObserving,
                                     base::Unretained(this)));
}

void TypedUrlChangeProcessor::StartObserving() {
  DCHECK(backend_loop_ == base::MessageLoop::current());
  DCHECK(profile_);
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
      content::Source<Profile>(profile_));
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
      content::Source<Profile>(profile_));
  notification_registrar_.Add(
      this, chrome::NOTIFICATION_HISTORY_URL_VISITED,
      content::Source<Profile>(profile_));
}

void TypedUrlChangeProcessor::StopObserving() {
  DCHECK(backend_loop_ == base::MessageLoop::current());
  DCHECK(profile_);
  notification_registrar_.Remove(
      this, chrome::NOTIFICATION_HISTORY_URLS_MODIFIED,
      content::Source<Profile>(profile_));
  notification_registrar_.Remove(
      this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
      content::Source<Profile>(profile_));
  notification_registrar_.Remove(
      this, chrome::NOTIFICATION_HISTORY_URL_VISITED,
      content::Source<Profile>(profile_));
}

}  // namespace browser_sync
