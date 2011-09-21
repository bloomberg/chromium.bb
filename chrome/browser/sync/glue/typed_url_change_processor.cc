// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_change_processor.h"

#include "base/location.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/internal_api/change_record.h"
#include "chrome/browser/sync/internal_api/read_node.h"
#include "chrome/browser/sync/internal_api/write_node.h"
#include "chrome/browser/sync/internal_api/write_transaction.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"

namespace browser_sync {

// This is the threshold at which we start throttling sync updates for typed
// URLs - any URLs with a typed_count >= this threshold will be throttled.
static const int kTypedUrlVisitThrottleThreshold = 10;

// This is the multiple we use when throttling sync updates. If the multiple is
// N, we sync up every Nth update (i.e. when typed_count % N == 0).
static const int kTypedUrlVisitThrottleMultiple = 10;

TypedUrlChangeProcessor::TypedUrlChangeProcessor(
    TypedUrlModelAssociator* model_associator,
    history::HistoryBackend* history_backend,
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      model_associator_(model_associator),
      history_backend_(history_backend),
      observing_(false),
      expected_loop_(MessageLoop::current()) {
  DCHECK(model_associator);
  DCHECK(history_backend);
  DCHECK(error_handler);
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  // When running in unit tests, there is already a NotificationService object.
  // Since only one can exist at a time per thread, check first.
  if (!NotificationService::current())
    notification_service_.reset(new NotificationService);
  StartObserving();
}

TypedUrlChangeProcessor::~TypedUrlChangeProcessor() {
  DCHECK(expected_loop_ == MessageLoop::current());
}

void TypedUrlChangeProcessor::Observe(int type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(expected_loop_ == MessageLoop::current());
  if (!observing_)
    return;

  VLOG(1) << "Observed typed_url change.";
  DCHECK(running());
  DCHECK(chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED == type ||
         chrome::NOTIFICATION_HISTORY_URLS_DELETED == type ||
         chrome::NOTIFICATION_HISTORY_URL_VISITED == type);
  if (type == chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED) {
    HandleURLsModified(Details<history::URLsModifiedDetails>(details).ptr());
  } else if (type == chrome::NOTIFICATION_HISTORY_URLS_DELETED) {
    HandleURLsDeleted(Details<history::URLsDeletedDetails>(details).ptr());
  } else if (type == chrome::NOTIFICATION_HISTORY_URL_VISITED) {
    HandleURLsVisited(Details<history::URLVisitedDetails>(details).ptr());
  }
}

void TypedUrlChangeProcessor::HandleURLsModified(
    history::URLsModifiedDetails* details) {

  sync_api::WriteTransaction trans(FROM_HERE, share_handle());
  for (std::vector<history::URLRow>::iterator url =
       details->changed_urls.begin(); url != details->changed_urls.end();
       ++url) {
    // Exit if we were unable to update the sync node.
    if (!CreateOrUpdateSyncNode(*url, &trans))
      return;
  }
}

bool TypedUrlChangeProcessor::CreateOrUpdateSyncNode(
    history::URLRow url, sync_api::WriteTransaction* trans) {
  // Get the visits for this node.
  history::VisitVector visit_vector;
  if (!TypedUrlModelAssociator::FixupURLAndGetVisits(
          history_backend_, &url, &visit_vector)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
                                          "Could not get the url's visits.");
    return false;
  }

  sync_api::ReadNode typed_url_root(trans);
  if (!typed_url_root.InitByTagLookup(kTypedUrlTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Server did not create the top-level typed_url node. We "
         "might be running against an out-of-date server.");
    return false;
  }

  std::string tag = url.url().spec();
  DCHECK(!visit_vector.empty());

  sync_api::WriteNode update_node(trans);
  if (update_node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
    model_associator_->WriteToSyncNode(url, visit_vector, &update_node);
  } else {
    sync_api::WriteNode create_node(trans);
    if (!create_node.InitUniqueByCreation(syncable::TYPED_URLS,
                                          typed_url_root, tag)) {
      error_handler()->OnUnrecoverableError(
          FROM_HERE, "Failed to create typed_url sync node.");
      return false;
    }

    create_node.SetTitle(UTF8ToWide(tag));
    model_associator_->WriteToSyncNode(url, visit_vector, &create_node);
    model_associator_->Associate(&tag, create_node.GetId());
  }
  return true;
}

void TypedUrlChangeProcessor::HandleURLsDeleted(
    history::URLsDeletedDetails* details) {
  sync_api::WriteTransaction trans(FROM_HERE, share_handle());

  if (details->all_history) {
    if (!model_associator_->DeleteAllNodes(&trans)) {
      error_handler()->OnUnrecoverableError(FROM_HERE, std::string());
      return;
    }
  } else {
    for (std::set<GURL>::iterator url = details->urls.begin();
         url != details->urls.end(); ++url) {
      sync_api::WriteNode sync_node(&trans);
      int64 sync_id = model_associator_->GetSyncIdFromChromeId(url->spec());
      if (sync_api::kInvalidId != sync_id) {
        if (!sync_node.InitByIdLookup(sync_id)) {
          error_handler()->OnUnrecoverableError(FROM_HERE,
              "Typed url node lookup failed.");
          return;
        }
        model_associator_->Disassociate(sync_node.GetId());
        sync_node.Remove();
      }
    }
  }
}

void TypedUrlChangeProcessor::HandleURLsVisited(
    history::URLVisitedDetails* details) {
  if (!ShouldSyncVisit(details))
    return;

  sync_api::WriteTransaction trans(FROM_HERE, share_handle());
  CreateOrUpdateSyncNode(details->row, &trans);
}

bool TypedUrlChangeProcessor::ShouldSyncVisit(
    history::URLVisitedDetails* details) {
  int typed_count = details->row.typed_count();
  PageTransition::Type transition = static_cast<PageTransition::Type>(
      details->transition & PageTransition::CORE_MASK);

  // Just use an ad-hoc criteria to determine whether to ignore this
  // notification. For most users, the distribution of visits is roughly a bell
  // curve with a long tail - there are lots of URLs with < 5 visits so we want
  // to make sure we sync up every visit to ensure the proper ordering of
  // suggestions. But there are relatively few URLs with > 10 visits, and those
  // tend to be more broadly distributed such that there's no need to sync up
  // every visit to preserve their relative ordering.
  return (transition == PageTransition::TYPED &&
          (typed_count < kTypedUrlVisitThrottleThreshold ||
           (typed_count % kTypedUrlVisitThrottleMultiple) == 0));
}

void TypedUrlChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::ImmutableChangeRecordList& changes) {
  DCHECK(expected_loop_ == MessageLoop::current());
  if (!running())
    return;

  sync_api::ReadNode typed_url_root(trans);
  if (!typed_url_root.InitByTagLookup(kTypedUrlTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "TypedUrl root node lookup failed.");
    return;
  }

  DCHECK(pending_titles_.empty() && pending_new_urls_.empty() &&
         pending_new_visits_.empty() && pending_deleted_visits_.empty() &&
         pending_updated_urls_.empty() && pending_deleted_urls_.empty());

  for (sync_api::ChangeRecordList::const_iterator it =
           changes.Get().begin(); it != changes.Get().end(); ++it) {
    if (sync_api::ChangeRecord::ACTION_DELETE ==
        it->action) {
      DCHECK(it->specifics.HasExtension(sync_pb::typed_url)) <<
          "Typed URL delete change does not have necessary specifics.";
      GURL url(it->specifics.GetExtension(sync_pb::typed_url).url());
      pending_deleted_urls_.push_back(url);
      // It's OK to disassociate here (before the items are actually deleted)
      // as we're guaranteed to either get a CommitChanges call or we'll hit
      // an unrecoverable error which will blow away the model associator.
      model_associator_->Disassociate(it->id);
      continue;
    }

    sync_api::ReadNode sync_node(trans);
    if (!sync_node.InitByIdLookup(it->id)) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "TypedUrl node lookup failed.");
      return;
    }

    // Check that the changed node is a child of the typed_urls folder.
    DCHECK(typed_url_root.GetId() == sync_node.GetParentId());
    DCHECK(syncable::TYPED_URLS == sync_node.GetModelType());

    const sync_pb::TypedUrlSpecifics& typed_url(
        sync_node.GetTypedUrlSpecifics());
    GURL url(typed_url.url());

    if (sync_api::ChangeRecord::ACTION_ADD == it->action) {
      DCHECK(typed_url.visits_size());
      if (!typed_url.visits_size()) {
        continue;
      }

      history::URLRow new_url(GURL(typed_url.url()));
      TypedUrlModelAssociator::UpdateURLRowFromTypedUrlSpecifics(
          typed_url, &new_url);

      model_associator_->Associate(&new_url.url().spec(), it->id);
      pending_new_urls_.push_back(new_url);

      std::vector<history::VisitInfo> added_visits;
      for (int c = 0; c < typed_url.visits_size(); ++c) {
        DCHECK(c == 0 || typed_url.visits(c) > typed_url.visits(c - 1));
        added_visits.push_back(history::VisitInfo(
            base::Time::FromInternalValue(typed_url.visits(c)),
            typed_url.visit_transitions(c)));
      }

      pending_new_visits_.push_back(
          std::pair<GURL, std::vector<history::VisitInfo> >(
              url, added_visits));
    } else {
      DCHECK_EQ(sync_api::ChangeRecord::ACTION_UPDATE, it->action);
      history::URLRow old_url;
      if (!history_backend_->GetURL(url, &old_url)) {
        LOG(ERROR) << "Could not fetch history row for " << url;
        continue;
      }

      history::VisitVector visits;
      if (!TypedUrlModelAssociator::FixupURLAndGetVisits(
              history_backend_, &old_url, &visits)) {
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "Could not get the url's visits.");
        return;
      }

      history::URLRow new_url(old_url);
      TypedUrlModelAssociator::UpdateURLRowFromTypedUrlSpecifics(
          typed_url, &new_url);

      pending_updated_urls_.push_back(
        std::pair<history::URLID, history::URLRow>(old_url.id(), new_url));

      if (old_url.title().compare(new_url.title()) != 0) {
        pending_titles_.push_back(
            std::pair<GURL, string16>(new_url.url(), new_url.title()));
      }

      std::vector<history::VisitInfo> added_visits;
      history::VisitVector removed_visits;
      TypedUrlModelAssociator::DiffVisits(visits, typed_url,
                                          &added_visits, &removed_visits);
      if (added_visits.size()) {
        pending_new_visits_.push_back(
            std::pair<GURL, std::vector<history::VisitInfo> >(
                url, added_visits));
      }
      if (removed_visits.size()) {
        pending_deleted_visits_.insert(pending_deleted_visits_.end(),
                                       removed_visits.begin(),
                                       removed_visits.end());
      }
    }
  }
}

void TypedUrlChangeProcessor::CommitChangesFromSyncModel() {
  DCHECK(expected_loop_ == MessageLoop::current());
  if (!running())
    return;

  StopObserving();
  if (!pending_deleted_urls_.empty())
    history_backend_->DeleteURLs(pending_deleted_urls_);

  if (!model_associator_->WriteToHistoryBackend(&pending_titles_,
                                                &pending_new_urls_,
                                                &pending_updated_urls_,
                                                &pending_new_visits_,
                                                &pending_deleted_visits_)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Could not write to the history backend.");
    return;
  }

  pending_titles_.clear();
  pending_new_urls_.clear();
  pending_updated_urls_.clear();
  pending_new_visits_.clear();
  pending_deleted_visits_.clear();
  pending_deleted_urls_.clear();

  StartObserving();
}

void TypedUrlChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(expected_loop_ == MessageLoop::current());
  observing_ = true;
}

void TypedUrlChangeProcessor::StopImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observing_ = false;
}


void TypedUrlChangeProcessor::StartObserving() {
  DCHECK(expected_loop_ == MessageLoop::current());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URL_VISITED,
                              NotificationService::AllSources());
}

void TypedUrlChangeProcessor::StopObserving() {
  DCHECK(expected_loop_ == MessageLoop::current());
  notification_registrar_.Remove(
      this, chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED,
      NotificationService::AllSources());
  notification_registrar_.Remove(this,
                                 chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                                 NotificationService::AllSources());
  notification_registrar_.Remove(this,
                                 chrome::NOTIFICATION_HISTORY_URL_VISITED,
                                 NotificationService::AllSources());
}

}  // namespace browser_sync
