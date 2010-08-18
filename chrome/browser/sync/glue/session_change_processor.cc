// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_change_processor.h"

#include <sstream>
#include <string>

#include "base/logging.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"

namespace browser_sync {

SessionChangeProcessor::SessionChangeProcessor(
    UnrecoverableErrorHandler* error_handler,
    SessionModelAssociator* session_model_associator)
    : ChangeProcessor(error_handler),
      session_model_associator_(session_model_associator),
      profile_(NULL) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(error_handler);
  DCHECK(session_model_associator_);
}

SessionChangeProcessor::~SessionChangeProcessor() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
}

void SessionChangeProcessor::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(running());
  DCHECK(profile_);
  switch (type.value) {
    case NotificationType::SESSION_SERVICE_SAVED: {
      std::string tag = session_model_associator_->GetCurrentMachineTag();
      DCHECK_EQ(Source<Profile>(source).ptr(), profile_);
      LOG(INFO) << "Got change notification of type " << type.value
                << " for session " << tag;
      session_model_associator_->UpdateSyncModelDataFromClient();
      break;
    }
    default:
      LOG(DFATAL) << "Received unexpected notification of type "
                  << type.value;
      break;
  }
}

void SessionChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  if (!running()) {
    return;
  }
  for (int i = 0; i < change_count; ++i) {
    const sync_api::SyncManager::ChangeRecord& change = changes[i];
    switch (change.action) {
      case sync_api::SyncManager::ChangeRecord::ACTION_ADD:
        UpdateModel(trans, change, true);
        break;
      case sync_api::SyncManager::ChangeRecord::ACTION_UPDATE:
        UpdateModel(trans, change, true);
        break;
      case sync_api::SyncManager::ChangeRecord::ACTION_DELETE:
        UpdateModel(trans, change, false);
        break;
    }
  }
}

void SessionChangeProcessor::UpdateModel(const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord& change, bool associate) {
  sync_api::ReadNode node(trans);
  if (!node.InitByIdLookup(change.id)) {
    std::stringstream error;
    error << "Session node lookup failed for change " << change.id
      << " of action type " << change.action;
    error_handler()->OnUnrecoverableError(FROM_HERE, error.str());
    return;
  }
  DCHECK_EQ(node.GetModelType(), syncable::SESSIONS);
  StopObserving();
  if (associate) {
    session_model_associator_->Associate(
      (const sync_pb::SessionSpecifics *) NULL, node.GetId());
  } else {
    session_model_associator_->Disassociate(node.GetId());
  }
  StartObserving();
}

void SessionChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(profile);
  DCHECK(profile_ == NULL);
  profile_ = profile;
  StartObserving();
}

void SessionChangeProcessor::StopImpl() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  StopObserving();
  profile_ = NULL;
}

void SessionChangeProcessor::StartObserving() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(profile_);
  LOG(INFO) << "Observing SESSION_SERVICE_SAVED";
  notification_registrar_.Add(
      this, NotificationType::SESSION_SERVICE_SAVED,
      Source<Profile>(profile_));
}

void SessionChangeProcessor::StopObserving() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(profile_);
  LOG(INFO) << "removing observation of all notifications";
  notification_registrar_.RemoveAll();
}

}  // namespace browser_sync

