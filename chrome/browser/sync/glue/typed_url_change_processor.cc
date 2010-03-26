// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_change_processor.h"

#include "base/string_util.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"

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
  DCHECK(!ChromeThread::CurrentlyOn(ChromeThread::UI));
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

  LOG(INFO) << "Observed typed_url change.";
  DCHECK(running());
  DCHECK(NotificationType::HISTORY_TYPED_URLS_MODIFIED == type ||
         NotificationType::HISTORY_URLS_DELETED == type);
  if (type == NotificationType::HISTORY_TYPED_URLS_MODIFIED) {
    HandleURLsModified(Details<history::URLsModifiedDetails>(details).ptr());
  } else if (type == NotificationType::HISTORY_URLS_DELETED) {
    HandleURLsDeleted(Details<history::URLsDeletedDetails>(details).ptr());
  }
}

void TypedUrlChangeProcessor::HandleURLsModified(
    history::URLsModifiedDetails* details) {
  sync_api::WriteTransaction trans(share_handle());

  // TODO(sync): Get visits without holding the write transaction.
  // See issue 34206

  sync_api::ReadNode typed_url_root(&trans);
  if (!typed_url_root.InitByTagLookup(kTypedUrlTag)) {
    error_handler()->OnUnrecoverableError();
    LOG(ERROR) << "Server did not create the top-level typed_url node. We "
               << "might be running against an out-of-date server.";
    return;
  }

  for (std::vector<history::URLRow>::iterator url =
       details->changed_urls.begin(); url != details->changed_urls.end();
       ++url) {
    std::string tag = url->url().spec();

    sync_api::WriteNode update_node(&trans);
    if (update_node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
      model_associator_->WriteToSyncNode(*url, &update_node);
    } else {
      sync_api::WriteNode create_node(&trans);
      if (!create_node.InitUniqueByCreation(syncable::TYPED_URLS,
                                            typed_url_root, tag)) {
        LOG(ERROR) << "Failed to create typed_url sync node.";
        error_handler()->OnUnrecoverableError();
        return;
      }

      create_node.SetTitle(UTF8ToWide(tag));
      model_associator_->WriteToSyncNode(*url, &create_node);

      model_associator_->Associate(&tag, create_node.GetId());
    }
  }
}

void TypedUrlChangeProcessor::HandleURLsDeleted(
    history::URLsDeletedDetails* details) {
  sync_api::WriteTransaction trans(share_handle());

  if (details->all_history) {
    if (!model_associator_->DeleteAllNodes(&trans)) {
      error_handler()->OnUnrecoverableError();
      return;
    }
  } else {
    for (std::set<GURL>::iterator url = details->urls.begin();
         url != details->urls.end(); ++url) {
      sync_api::WriteNode sync_node(&trans);
      int64 sync_id =
       model_associator_->GetSyncIdFromChromeId(url->spec());
      if (sync_api::kInvalidId == sync_id) {
       LOG(ERROR) << "Unexpected notification for: " <<
         url->spec();
       error_handler()->OnUnrecoverableError();
       return;
      } else {
        if (!sync_node.InitByIdLookup(sync_id)) {
          LOG(ERROR) << "Typed url node lookup failed.";
          error_handler()->OnUnrecoverableError();
          return;
        }
        model_associator_->Disassociate(sync_node.GetId());
        sync_node.Remove();
      }
    }
  }
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
    LOG(ERROR) << "TypedUrl root node lookup failed.";
    error_handler()->OnUnrecoverableError();
    return;
  }

  TypedUrlModelAssociator::TypedUrlTitleVector titles;
  TypedUrlModelAssociator::TypedUrlVector new_urls;
  TypedUrlModelAssociator::TypedUrlUpdateVector updated_urls;

  for (int i = 0; i < change_count; ++i) {

    sync_api::ReadNode sync_node(trans);
    if (!sync_node.InitByIdLookup(changes[i].id)) {
      LOG(ERROR) << "TypedUrl node lookup failed.";
      error_handler()->OnUnrecoverableError();
      return;
    }

    // Check that the changed node is a child of the typed_urls folder.
    DCHECK(typed_url_root.GetId() == sync_node.GetParentId());
    DCHECK(syncable::TYPED_URLS == sync_node.GetModelType());

    const sync_pb::TypedUrlSpecifics& typed_url(
        sync_node.GetTypedUrlSpecifics());
    if (sync_api::SyncManager::ChangeRecord::ACTION_ADD == changes[i].action) {
      history::URLRow new_url(GURL(typed_url.url()));
      new_url.set_title(UTF8ToWide(typed_url.title()));
      new_url.set_visit_count(typed_url.visit_count());
      new_url.set_typed_count(typed_url.typed_count());
      new_url.set_last_visit(
          base::Time::FromInternalValue(typed_url.last_visit()));
      new_url.set_hidden(typed_url.hidden());

      new_urls.push_back(new_url);
    } else if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
               changes[i].action) {
      history_backend_->DeleteURL(GURL(typed_url.url()));
    } else {
      history::URLRow old_url;
      if (!history_backend_->GetURL(GURL(typed_url.url()), &old_url)) {
        LOG(ERROR) << "TypedUrl db lookup failed.";
        error_handler()->OnUnrecoverableError();
        return;
      }

      history::URLRow new_url(GURL(typed_url.url()));
      new_url.set_title(UTF8ToWide(typed_url.title()));
      new_url.set_visit_count(typed_url.visit_count());
      new_url.set_typed_count(typed_url.typed_count());
      new_url.set_last_visit(
          base::Time::FromInternalValue(typed_url.last_visit()));
      new_url.set_hidden(typed_url.hidden());

      updated_urls.push_back(
        std::pair<history::URLID, history::URLRow>(old_url.id(), new_url));

      if (old_url.title().compare(new_url.title()) != 0) {
        titles.push_back(std::pair<GURL, std::wstring>(new_url.url(),
                                                       new_url.title()));
      }
    }
  }
  model_associator_->WriteToHistoryBackend(&titles, &new_urls, &updated_urls);

  StartObserving();
}

void TypedUrlChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(expected_loop_ == MessageLoop::current());
  observing_ = true;
}

void TypedUrlChangeProcessor::StopImpl() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  observing_ = false;
}


void TypedUrlChangeProcessor::StartObserving() {
  DCHECK(expected_loop_ == MessageLoop::current());
  notification_registrar_.Add(this,
                              NotificationType::HISTORY_TYPED_URLS_MODIFIED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
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
}

}  // namespace browser_sync
