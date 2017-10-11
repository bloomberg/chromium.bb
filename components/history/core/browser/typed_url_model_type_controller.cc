// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/typed_url_model_type_controller.h"

#include "base/bind.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_client.h"

using syncer::ModelType;
using syncer::ModelTypeController;
using syncer::ModelTypeSyncBridge;
using syncer::SyncClient;

namespace history {

namespace {

// The history service exposes a special non-standard task API which calls back
// once a task has been dispatched, so we have to build a special wrapper around
// the tasks we want to run.
class RunTaskOnHistoryThread : public HistoryDBTask {
 public:
  explicit RunTaskOnHistoryThread(const base::Closure& task) : task_(task) {}

  bool RunOnDBThread(HistoryBackend* backend, HistoryDatabase* db) override {
    // Invoke the task, then free it immediately so we don't keep a reference
    // around all the way until DoneRunOnMainThread() is invoked back on the
    // main thread - we want to release references as soon as possible to avoid
    // keeping them around too long during shutdown.
    task_.Run();
    task_.Reset();
    return true;
  }

  void DoneRunOnMainThread() override {}

 protected:
  base::Closure task_;
};

}  // namespace

TypedURLModelTypeController::TypedURLModelTypeController(
    SyncClient* sync_client,
    const char* history_disabled_pref_name)
    : ModelTypeController(syncer::TYPED_URLS, sync_client, nullptr),
      history_disabled_pref_name_(history_disabled_pref_name) {
  pref_registrar_.Init(sync_client->GetPrefService());
  pref_registrar_.Add(
      history_disabled_pref_name_,
      base::Bind(
          &TypedURLModelTypeController::OnSavingBrowserHistoryDisabledChanged,
          base::AsWeakPtr(this)));
}

TypedURLModelTypeController::~TypedURLModelTypeController() {}

bool TypedURLModelTypeController::ReadyForStart() const {
  return !sync_client()->GetPrefService()->GetBoolean(
      history_disabled_pref_name_);
}

void TypedURLModelTypeController::PostBridgeTask(const base::Location& location,
                                                 const BridgeTask& task) {
  history::HistoryService* history = sync_client()->GetHistoryService();
  if (!history) {
    // History must be disabled - don't start.
    LOG(WARNING) << "Cannot access history service.";
    return;
  }

  TypedURLSyncBridge* bridge = history->GetTypedURLSyncBridge();
  PostTaskOnHistoryThread(base::Bind(task, bridge));
}

void TypedURLModelTypeController::OnSavingBrowserHistoryDisabledChanged() {
  if (sync_client()->GetPrefService()->GetBoolean(
          history_disabled_pref_name_)) {
    // We've turned off history persistence, so if we are running,
    // generate an unrecoverable error. This can be fixed by restarting
    // Chrome (on restart, typed urls will not be a registered type).
    if (state() != NOT_RUNNING && state() != STOPPING) {
      PostTaskOnHistoryThread(base::Bind(
          &TypedURLModelTypeController::ReportModelError, base::AsWeakPtr(this),
          syncer::ModelError(FROM_HERE,
                             "History saving is now disabled by policy.")));
    }
  }
}

void TypedURLModelTypeController::PostTaskOnHistoryThread(
    const base::Closure& task) {
  history::HistoryService* history = sync_client()->GetHistoryService();
  if (!history) {
    // History must be disabled - don't start.
    LOG(WARNING) << "Cannot access history service.";
    return;
  }

  history->ScheduleDBTask(std::make_unique<RunTaskOnHistoryThread>(task),
                          &task_tracker_);
}

}  // namespace history
