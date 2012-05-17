// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_data_type_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"

namespace browser_sync {

using content::BrowserThread;

namespace {

// The history service exposes a special non-standard task API which calls back
// once a task has been dispatched, so we have to build a special wrapper around
// the tasks we want to run.
class RunTaskOnHistoryThread : public HistoryDBTask {
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
    : NonFrontendDataTypeController(profile_sync_factory,
                                    profile,
                                    sync_service),
      backend_(NULL) {
  pref_registrar_.Init(profile->GetPrefs());
  pref_registrar_.Add(prefs::kSavingBrowserHistoryDisabled, this);
}

syncable::ModelType TypedUrlDataTypeController::type() const {
  return syncable::TYPED_URLS;
}

browser_sync::ModelSafeGroup TypedUrlDataTypeController::model_safe_group()
    const {
  return browser_sync::GROUP_HISTORY;
}

void TypedUrlDataTypeController::SetBackend(history::HistoryBackend* backend) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  backend_ = backend;
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
          profile_sync_service()->OnDisableDatatype(syncable::TYPED_URLS,
              FROM_HERE, "History saving is now disabled by policy.");
        }
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool TypedUrlDataTypeController::PostTaskOnBackendThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  HistoryService* history = profile()->GetHistoryService(
      Profile::IMPLICIT_ACCESS);
  if (history) {
    history->ScheduleDBTask(new RunTaskOnHistoryThread(task, this),
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

void TypedUrlDataTypeController::StopModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(state() == STOPPING || state() == NOT_RUNNING || state() == DISABLED);
  DVLOG(1) << "TypedUrlDataTypeController::StopModels(): State = " << state();
  notification_registrar_.RemoveAll();
}

TypedUrlDataTypeController::~TypedUrlDataTypeController() {}

}  // namespace browser_sync
