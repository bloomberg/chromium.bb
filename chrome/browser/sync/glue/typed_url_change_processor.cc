// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_change_processor.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace browser_sync {

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

void TypedUrlChangeProcessor::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(expected_loop_ == MessageLoop::current());
  if (!observing_)
    return;

  VLOG(1) << "Observed typed_url change.";
  DCHECK(running());
  DCHECK(NotificationType::HISTORY_TYPED_URLS_MODIFIED == type ||
         NotificationType::HISTORY_URLS_DELETED == type ||
         NotificationType::HISTORY_URL_VISITED == type);
  if (type == NotificationType::HISTORY_TYPED_URLS_MODIFIED) {
    HandleURLsModified(Details<history::URLsModifiedDetails>(details).ptr());
  } else if (type == NotificationType::HISTORY_URLS_DELETED) {
    HandleURLsDeleted(Details<history::URLsDeletedDetails>(details).ptr());
  } else if (type == NotificationType::HISTORY_URL_VISITED) {
    HandleURLsVisited(Details<history::URLVisitedDetails>(details).ptr());
  }
}

void TypedUrlChangeProcessor::HandleURLsModified(
    history::URLsModifiedDetails* details) {
  // Get all the visits.
  std::map<history::URLID, history::VisitVector> visit_vectors;
  for (std::vector<history::URLRow>::iterator url =
       details->changed_urls.begin(); url != details->changed_urls.end();
       ++url) {
    if (!history_backend_->GetVisitsForURL(url->id(),
                                           &(visit_vectors[url->id()]))) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "Could not get the url's visits.");
      return;
    }
    DCHECK(!visit_vectors[url->id()].empty());
  }

  sync_api::WriteTransaction trans(share_handle());

  sync_api::ReadNode typed_url_root(&trans);
  if (!typed_url_root.InitByTagLookup(kTypedUrlTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Server did not create the top-level typed_url node. We "
         "might be running against an out-of-date server.");
    return;
  }

  for (std::vector<history::URLRow>::iterator url =
       details->changed_urls.begin(); url != details->changed_urls.end();
       ++url) {
    std::string tag = url->url().spec();

    history::VisitVector& visits = visit_vectors[url->id()];

    DCHECK(!visits.empty());

    DCHECK(static_cast<size_t>(url->visit_count()) == visits.size());
    if (static_cast<size_t>(url->visit_count()) != visits.size()) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "Visit count does not match.");
      return;
    }

    sync_api::WriteNode update_node(&trans);
    if (update_node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
      model_associator_->WriteToSyncNode(*url, visits, &update_node);
    } else {
      sync_api::WriteNode create_node(&trans);
      if (!create_node.InitUniqueByCreation(syncable::TYPED_URLS,
                                            typed_url_root, tag)) {
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "Failed to create typed_url sync node.");
        return;
      }

      create_node.SetTitle(UTF8ToWide(tag));
      model_associator_->WriteToSyncNode(*url, visits, &create_node);

      model_associator_->Associate(&tag, create_node.GetId());
    }
  }
}

void TypedUrlChangeProcessor::HandleURLsDeleted(
    history::URLsDeletedDetails* details) {
  sync_api::WriteTransaction trans(share_handle());

  if (details->all_history) {
    if (!model_associator_->DeleteAllNodes(&trans)) {
      error_handler()->OnUnrecoverableError(FROM_HERE, std::string());
      return;
    }
  } else {
    for (std::set<GURL>::iterator url = details->urls.begin();
         url != details->urls.end(); ++url) {
      sync_api::WriteNode sync_node(&trans);
      int64 sync_id =
      model_associator_->GetSyncIdFromChromeId(url->spec());
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
  if (!details->row.typed_count()) {
    // We only care about typed urls.
    return;
  }
  history::VisitVector visits;
  if (!history_backend_->GetVisitsForURL(details->row.id(), &visits) ||
      visits.empty()) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Could not get the url's visits.");
    return;
  }

  DCHECK(static_cast<size_t>(details->row.visit_count()) == visits.size());

  sync_api::WriteTransaction trans(share_handle());
  std::string tag = details->row.url().spec();
  sync_api::WriteNode update_node(&trans);
  if (!update_node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
    // If we don't know about it yet, it will be added later.
    return;
  }
  sync_pb::TypedUrlSpecifics typed_url(update_node.GetTypedUrlSpecifics());
  typed_url.add_visit(visits.back().visit_time.ToInternalValue());
  update_node.SetTypedUrlSpecifics(typed_url);
}

void TypedUrlChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(expected_loop_ == MessageLoop::current());
  if (!running())
    return;
  StopObserving();

  sync_api::ReadNode typed_url_root(trans);
  if (!typed_url_root.InitByTagLookup(kTypedUrlTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "TypedUrl root node lookup failed.");
    return;
  }

  TypedUrlModelAssociator::TypedUrlTitleVector titles;
  TypedUrlModelAssociator::TypedUrlVector new_urls;
  TypedUrlModelAssociator::TypedUrlVisitVector new_visits;
  history::VisitVector deleted_visits;
  TypedUrlModelAssociator::TypedUrlUpdateVector updated_urls;

  for (int i = 0; i < change_count; ++i) {
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      DCHECK(changes[i].specifics.HasExtension(sync_pb::typed_url)) <<
          "Typed URL delete change does not have necessary specifics.";
      GURL url(changes[i].specifics.GetExtension(sync_pb::typed_url).url());
      history_backend_->DeleteURL(url);
      continue;
    }

    sync_api::ReadNode sync_node(trans);
    if (!sync_node.InitByIdLookup(changes[i].id)) {
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

    if (sync_api::SyncManager::ChangeRecord::ACTION_ADD == changes[i].action) {
      DCHECK(typed_url.visit_size());
      if (!typed_url.visit_size()) {
        continue;
      }

      history::URLRow new_url(url);
      new_url.set_title(UTF8ToUTF16(typed_url.title()));

      // When we add a new url, the last visit is always added, thus we set
      // the initial visit count to one.  This value will be automatically
      // incremented as visits are added.
      new_url.set_visit_count(1);
      new_url.set_typed_count(typed_url.typed_count());
      new_url.set_hidden(typed_url.hidden());

      new_url.set_last_visit(base::Time::FromInternalValue(
          typed_url.visit(typed_url.visit_size() - 1)));

      new_urls.push_back(new_url);

      // The latest visit gets added automatically, so skip it.
      std::vector<base::Time> added_visits;
      for (int c = 0; c < typed_url.visit_size() - 1; ++c) {
        DCHECK(typed_url.visit(c) < typed_url.visit(c + 1));
        added_visits.push_back(
            base::Time::FromInternalValue(typed_url.visit(c)));
      }

      new_visits.push_back(
          std::pair<GURL, std::vector<base::Time> >(url, added_visits));
    } else {
      history::URLRow old_url;
      if (!history_backend_->GetURL(url, &old_url)) {
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "TypedUrl db lookup failed.");
        return;
      }

      history::VisitVector visits;
      if (!history_backend_->GetVisitsForURL(old_url.id(), &visits)) {
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "Could not get the url's visits.");
        return;
      }

      history::URLRow new_url(url);
      new_url.set_title(UTF8ToUTF16(typed_url.title()));
      new_url.set_visit_count(old_url.visit_count());
      new_url.set_typed_count(typed_url.typed_count());
      new_url.set_last_visit(old_url.last_visit());
      new_url.set_hidden(typed_url.hidden());

      updated_urls.push_back(
        std::pair<history::URLID, history::URLRow>(old_url.id(), new_url));

      if (old_url.title().compare(new_url.title()) != 0) {
        titles.push_back(std::pair<GURL, string16>(new_url.url(),
                                                   new_url.title()));
      }

      std::vector<base::Time> added_visits;
      history::VisitVector removed_visits;
      TypedUrlModelAssociator::DiffVisits(visits, typed_url,
                                          &added_visits, &removed_visits);
      if (added_visits.size()) {
        new_visits.push_back(
          std::pair<GURL, std::vector<base::Time> >(url, added_visits));
      }
      if (removed_visits.size()) {
        deleted_visits.insert(deleted_visits.end(), removed_visits.begin(),
                              removed_visits.end());
      }
    }
  }
  if (!model_associator_->WriteToHistoryBackend(&titles, &new_urls,
                                                &updated_urls,
                                                &new_visits, &deleted_visits)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Could not write to the history backend.");
    return;
  }

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
                              NotificationType::HISTORY_TYPED_URLS_MODIFIED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::HISTORY_URL_VISITED,
                              NotificationService::AllSources());
}

void TypedUrlChangeProcessor::StopObserving() {
  DCHECK(expected_loop_ == MessageLoop::current());
  notification_registrar_.Remove(this,
                                 NotificationType::HISTORY_TYPED_URLS_MODIFIED,
                                 NotificationService::AllSources());
  notification_registrar_.Remove(this,
                                 NotificationType::HISTORY_URLS_DELETED,
                                 NotificationService::AllSources());
  notification_registrar_.Remove(this,
                                 NotificationType::HISTORY_URL_VISITED,
                                 NotificationService::AllSources());
}

}  // namespace browser_sync
