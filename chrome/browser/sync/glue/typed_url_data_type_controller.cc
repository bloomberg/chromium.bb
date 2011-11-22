// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "base/task.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"

using content::BrowserThread;

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
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile)
    : NonFrontendDataTypeController(profile_sync_factory,
                                 profile),
      backend_(NULL) {
  pref_registrar_.Init(profile->GetPrefs());
  pref_registrar_.Add(prefs::kSavingBrowserHistoryDisabled, this);
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

bool TypedUrlDataTypeController::StartAssociationAsync() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  HistoryService* history = profile()->GetHistoryService(
      Profile::IMPLICIT_ACCESS);
  if (history) {
    history_service_ = history;
    history_service_->ScheduleDBTask(new ControlTask(this, true),
                                     &cancelable_consumer_);
    return true;
  } else {
    // History must be disabled - don't start.
    LOG(WARNING) << "Cannot access history service - disabling typed url sync";
    return false;
  }
}

void TypedUrlDataTypeController::CreateSyncComponents() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  DCHECK(backend_);
  ProfileSyncComponentsFactory::SyncComponents sync_components =
      profile_sync_factory()->CreateTypedUrlSyncComponents(
          profile_sync_service(),
          backend_,
          this);
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

void TypedUrlDataTypeController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED:
      DCHECK(*content::Details<std::string>(details).ptr() ==
             prefs::kSavingBrowserHistoryDisabled);
      if (profile()->GetPrefs()->GetBoolean(
              prefs::kSavingBrowserHistoryDisabled)) {
        // We've turned off history persistence, so if we are running,
        // generate an unrecoverable error. This can be fixed by restarting
        // Chrome (on restart, typed urls will not be a registered type).
        if (state() != NOT_RUNNING && state() != STOPPING) {
          profile_sync_service()->OnUnrecoverableError(
              FROM_HERE, "History saving is now disabled by policy.");
        }
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

void TypedUrlDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state() == STOPPING || state() == NOT_RUNNING || state() == DISABLED);
  VLOG(1) << "TypedUrlDataTypeController::StopModels(): State = " << state();
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
