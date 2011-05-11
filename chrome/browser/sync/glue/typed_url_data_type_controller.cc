// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "base/task.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"

namespace browser_sync {

class ControlTask : public HistoryDBTask {
 public:
  ControlTask(TypedUrlDataTypeController* controller, bool start)
    : controller_(controller), start_(start) {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) {
    controller_->RunOnHistoryThread(start_, backend);

    // Release the reference to the controller.  This ensures that
    // the controller isn't held past its lifetime in unit tests.
    controller_ = NULL;
    return true;
  }

  virtual void DoneRunOnMainThread() {}

 protected:
  scoped_refptr<TypedUrlDataTypeController> controller_;
  bool start_;
};

TypedUrlDataTypeController::TypedUrlDataTypeController(
    ProfileSyncFactory* profile_sync_factory,
    Profile* profile)
    : NonFrontendDataTypeController(profile_sync_factory,
                                 profile),
      backend_(NULL) {
}

TypedUrlDataTypeController::~TypedUrlDataTypeController() {
}

void TypedUrlDataTypeController::RunOnHistoryThread(bool start,
    history::HistoryBackend* backend) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  // The only variable we can access here is backend_, since it is always
  // read from the DB thread. Touching anything else could lead to memory
  // corruption.
  backend_ = backend;
  if (start) {
    StartAssociation();
  } else {
    StopAssociation();
  }
  backend_ = NULL;
}

bool TypedUrlDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);
  HistoryService* history = profile()->GetHistoryServiceWithoutCreating();
  if (history) {
    history_service_ = history;
    return true;
  } else {
    notification_registrar_.Add(this, NotificationType::HISTORY_LOADED,
                                NotificationService::AllSources());
    return false;
  }
}

bool TypedUrlDataTypeController::StartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  DCHECK(history_service_.get());
  history_service_->ScheduleDBTask(new ControlTask(this, true),
                                   &cancelable_consumer_);
  return true;
}

void TypedUrlDataTypeController::CreateSyncComponents() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  DCHECK(backend_);
  ProfileSyncFactory::SyncComponents sync_components =
      profile_sync_factory()->CreateTypedUrlSyncComponents(
          profile_sync_service(),
          backend_,
          this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

void TypedUrlDataTypeController::Observe(NotificationType type,
                                         const NotificationSource& source,
                                         const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), MODEL_STARTING);
  notification_registrar_.Remove(this,
                                 NotificationType::HISTORY_LOADED,
                                 NotificationService::AllSources());
  history_service_ = profile()->GetHistoryServiceWithoutCreating();
  DCHECK(history_service_.get());
  set_state(ASSOCIATING);
  StopAssociationAsync();
}

void TypedUrlDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state() == STOPPING || state() == NOT_RUNNING);
  notification_registrar_.RemoveAll();
}

bool TypedUrlDataTypeController::StopAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), STOPPING);
  DCHECK(history_service_.get());
  history_service_->ScheduleDBTask(new ControlTask(this, false),
                                   &cancelable_consumer_);
  return true;
}

syncable::ModelType TypedUrlDataTypeController::type() const {
  return syncable::TYPED_URLS;
}

browser_sync::ModelSafeGroup TypedUrlDataTypeController::model_safe_group()
    const {
  return browser_sync::GROUP_HISTORY;
}

void TypedUrlDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_COUNTS("Sync.TypedUrlRunFailures", 1);
}

void TypedUrlDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_TIMES("Sync.TypedUrlAssociationTime", time);
}

void TypedUrlDataTypeController::RecordStartFailure(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Sync.TypedUrlStartFailures",
                            result,
                            MAX_START_RESULT);
}
}  // namespace browser_sync
