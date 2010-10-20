// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/session_change_processor.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/scoped_vector.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/sync/engine/syncapi.h"
#include "chrome/browser/sync/glue/session_model_associator.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"

namespace browser_sync {

SessionChangeProcessor::SessionChangeProcessor(
    UnrecoverableErrorHandler* error_handler,
    SessionModelAssociator* session_model_associator)
    : ChangeProcessor(error_handler),
      session_model_associator_(session_model_associator),
      profile_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(error_handler);
  DCHECK(session_model_associator_);
}

SessionChangeProcessor::~SessionChangeProcessor() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void SessionChangeProcessor::Observe(NotificationType type,
                                     const NotificationSource& source,
                                     const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(running());
  DCHECK(profile_);
  switch (type.value) {
    case NotificationType::SESSION_SERVICE_SAVED: {
      std::string tag = session_model_associator_->GetCurrentMachineTag();
      DCHECK_EQ(Source<Profile>(source).ptr(), profile_);
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!running()) {
    return;
  }

  StopObserving();

  // This currently ignores the tracked changes and rebuilds the sessions from
  // all the session sync nodes.
  // TODO(zea): Make use of |changes| to adjust only modified sessions.
  session_model_associator_->UpdateFromSyncModel(trans);

  // Notify foreign session handlers that there are new sessions.
  NotificationService::current()->Notify(
      NotificationType::FOREIGN_SESSION_UPDATED,
      NotificationService::AllSources(),
      NotificationService::NoDetails());

  StartObserving();
}

void SessionChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  DCHECK(profile_ == NULL);
  profile_ = profile;
  StartObserving();
}

void SessionChangeProcessor::StopImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  StopObserving();
  profile_ = NULL;
}

void SessionChangeProcessor::StartObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  notification_registrar_.Add(
      this, NotificationType::SESSION_SERVICE_SAVED,
      Source<Profile>(profile_));
}

void SessionChangeProcessor::StopObserving() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  notification_registrar_.RemoveAll();
}

}  // namespace browser_sync
