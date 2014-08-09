// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/glue/typed_url_change_processor.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"

namespace browser_sync {

using content::BrowserThread;

namespace {

// The history service exposes a special non-standard task API which calls back
// once a task has been dispatched, so we have to build a special wrapper around
// the tasks we want to run.
class RunTaskOnHistoryThread : public history::HistoryDBTask {
 public:
  explicit RunTaskOnHistoryThread(const base::Closure& task,
                                  TypedUrlDataTypeController* dtc)
      : task_(new base::Closure(task)),
        dtc_(dtc) {
  }

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    // Set the backend, then release our reference before executing the task.
    dtc_->SetBackend(backend);
    dtc_ = NULL;

    // Invoke the task, then free it immediately so we don't keep a reference
    // around all the way until DoneRunOnMainThread() is invoked back on the
    // main thread - we want to release references as soon as possible to avoid
    // keeping them around too long during shutdown.
    task_->Run();
    task_.reset();
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {}

 protected:
  virtual ~RunTaskOnHistoryThread() {}

  scoped_ptr<base::Closure> task_;
  scoped_refptr<TypedUrlDataTypeController> dtc_;
};

}  // namespace

TypedUrlDataTypeController::TypedUrlDataTypeController(
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : NonFrontendDataTypeController(
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI),
          base::Bind(&ChromeReportUnrecoverableError),
          profile_sync_factory,
          profile,
          sync_service),
      backend_(NULL) {
  pref_registrar_.Init(profile->GetPrefs());
  pref_registrar_.Add(
      prefs::kSavingBrowserHistoryDisabled,
      base::Bind(
          &TypedUrlDataTypeController::OnSavingBrowserHistoryDisabledChanged,
          base::Unretained(this)));
}

syncer::ModelType TypedUrlDataTypeController::type() const {
  return syncer::TYPED_URLS;
}

syncer::ModelSafeGroup TypedUrlDataTypeController::model_safe_group()
    const {
  return syncer::GROUP_HISTORY;
}

void TypedUrlDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  if (profile()->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled)) {
    model_load_callback.Run(
        type(),
        syncer::SyncError(FROM_HERE,
                          syncer::SyncError::DATATYPE_ERROR,
                          "History sync disabled by policy.",
                          type()));
    return;
  }

  set_state(MODEL_LOADED);
  model_load_callback.Run(type(), syncer::SyncError());
}

void TypedUrlDataTypeController::SetBackend(history::HistoryBackend* backend) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  backend_ = backend;
}

void TypedUrlDataTypeController::OnSavingBrowserHistoryDisabledChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (profile()->GetPrefs()->GetBoolean(
          prefs::kSavingBrowserHistoryDisabled)) {
    // We've turned off history persistence, so if we are running,
    // generate an unrecoverable error. This can be fixed by restarting
    // Chrome (on restart, typed urls will not be a registered type).
    if (state() != NOT_RUNNING && state() != STOPPING) {
      syncer::SyncError error(
          FROM_HERE,
          syncer::SyncError::DATATYPE_POLICY_ERROR,
          "History saving is now disabled by policy.",
          syncer::TYPED_URLS);
      OnSingleDataTypeUnrecoverableError(error);
    }
  }
}

bool TypedUrlDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile(), Profile::IMPLICIT_ACCESS);
  if (history) {
    history->ScheduleDBTask(
        scoped_ptr<history::HistoryDBTask>(
            new RunTaskOnHistoryThread(task, this)),
        &task_tracker_);
    return true;
  } else {
    // History must be disabled - don't start.
    LOG(WARNING) << "Cannot access history service - disabling typed url sync";
    return false;
  }
}

ProfileSyncComponentsFactory::SyncComponents
TypedUrlDataTypeController::CreateSyncComponents() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state(), ASSOCIATING);
  DCHECK(backend_);
  return profile_sync_factory()->CreateTypedUrlSyncComponents(
      profile_sync_service(),
      backend_,
      this);
}

void TypedUrlDataTypeController::DisconnectProcessor(
    sync_driver::ChangeProcessor* processor) {
  static_cast<TypedUrlChangeProcessor*>(processor)->Disconnect();
}

TypedUrlDataTypeController::~TypedUrlDataTypeController() {}

}  // namespace browser_sync
