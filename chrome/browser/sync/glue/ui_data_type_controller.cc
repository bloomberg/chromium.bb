// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/ui_data_type_controller.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/shared_change_processor_ref.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/api/syncable_service.h"
#include "sync/syncable/model_type.h"
#include "sync/util/data_type_histogram.h"

using content::BrowserThread;

namespace browser_sync {

UIDataTypeController::UIDataTypeController()
    : profile_sync_factory_(NULL),
      profile_(NULL),
      sync_service_(NULL),
      state_(NOT_RUNNING),
      type_(syncable::UNSPECIFIED) {
}

UIDataTypeController::UIDataTypeController(
    syncable::ModelType type,
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      sync_service_(sync_service),
      state_(NOT_RUNNING),
      type_(type) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_sync_factory);
  DCHECK(profile);
  DCHECK(sync_service);
  DCHECK(syncable::IsRealDataType(type_));
}

UIDataTypeController::~UIDataTypeController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void UIDataTypeController::Start(const StartCallback& start_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!start_callback.is_null());
  DCHECK(syncable::IsRealDataType(type_));
  if (state_ != NOT_RUNNING) {
    start_callback.Run(BUSY, SyncError());
    return;
  }

  start_callback_ = start_callback;

  // Since we can't be called multiple times before Stop() is called,
  // |shared_change_processor_| must be NULL here.
  DCHECK(!shared_change_processor_.get());
  shared_change_processor_ =
      profile_sync_factory_->CreateSharedChangeProcessor();
  DCHECK(shared_change_processor_.get());

  state_ = MODEL_STARTING;
  if (!StartModels()) {
    // If we are waiting for some external service to load before associating
    // or we failed to start the models, we exit early. state_ will control
    // what we perform next.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING);
    return;
  }

  state_ = ASSOCIATING;
  Associate();
  // It's possible StartDone(..) resulted in a Stop() call, or that association
  // failed, so we just verify that the state has moved foward.
  DCHECK_NE(state_, ASSOCIATING);
}

bool UIDataTypeController::StartModels() {
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association.
  return true;
}

void UIDataTypeController::Associate() {
  DCHECK_EQ(state_, ASSOCIATING);

  // Connect |shared_change_processor_| to the syncer and get the
  // SyncableService associated with type().
  local_service_ = shared_change_processor_->Connect(profile_sync_factory_,
                                                     sync_service_,
                                                     this,
                                                     type());
  if (!local_service_.get()) {
    SyncError error(FROM_HERE, "Failed to connect to syncer.", type());
    StartFailed(UNRECOVERABLE_ERROR, error);
    return;
  }

  if (!shared_change_processor_->CryptoReadyIfNecessary()) {
    StartFailed(NEEDS_CRYPTO, SyncError());
    return;
  }

  bool sync_has_nodes = false;
  if (!shared_change_processor_->SyncModelHasUserCreatedNodes(
          &sync_has_nodes)) {
    SyncError error(FROM_HERE, "Failed to load sync nodes", type());
    StartFailed(UNRECOVERABLE_ERROR, error);
    return;
  }

  base::TimeTicks start_time = base::TimeTicks::Now();
  SyncError error;
  SyncDataList initial_sync_data;
  error = shared_change_processor_->GetSyncData(&initial_sync_data);
  if (error.IsSet()) {
    StartFailed(ASSOCIATION_FAILED, error);
    return;
  }

  // Passes a reference to |shared_change_processor_|.
  error = local_service_->MergeDataAndStartSyncing(
      type(),
      initial_sync_data,
      scoped_ptr<SyncChangeProcessor>(
          new SharedChangeProcessorRef(shared_change_processor_)),
      scoped_ptr<SyncErrorFactory>(
          new SharedChangeProcessorRef(shared_change_processor_)));
  RecordAssociationTime(base::TimeTicks::Now() - start_time);
  if (error.IsSet()) {
    StartFailed(ASSOCIATION_FAILED, error);
    return;
  }

  shared_change_processor_->ActivateDataType(model_safe_group());
  state_ = RUNNING;
  StartDone(sync_has_nodes ? OK : OK_FIRST_RUN);
}

void UIDataTypeController::StartFailed(StartResult result,
                                       const SyncError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (IsUnrecoverableResult(result))
    RecordUnrecoverableError(FROM_HERE, "StartFailed");
  StopModels();
  if (result == ASSOCIATION_FAILED) {
    state_ = DISABLED;
  } else {
    state_ = NOT_RUNNING;
  }
  RecordStartFailure(result);

  if (shared_change_processor_.get()) {
    shared_change_processor_->Disconnect();
    shared_change_processor_ = NULL;
  }

  // We have to release the callback before we call it, since it's possible
  // invoking the callback will trigger a call to Stop(), which will get
  // confused by the non-NULL start_callback_.
  StartCallback callback = start_callback_;
  start_callback_.Reset();
  callback.Run(result, error);
}

void UIDataTypeController::StartDone(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We have to release the callback before we call it, since it's possible
  // invoking the callback will trigger a call to Stop(), which will get
  // confused by the non-NULL start_callback_.
  StartCallback callback = start_callback_;
  start_callback_.Reset();
  callback.Run(result, SyncError());
}

void UIDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(syncable::IsRealDataType(type_));

  State prev_state = state_;
  state_ = STOPPING;

  if (shared_change_processor_.get()) {
    shared_change_processor_->Disconnect();
    shared_change_processor_ = NULL;
  }

  // If Stop() is called while Start() is waiting for the datatype model to
  // load, abort the start.
  if (prev_state == MODEL_STARTING) {
    StartFailed(ABORTED, SyncError());
    // We can just return here since we haven't performed association if we're
    // still in MODEL_STARTING.
    return;
  }
  DCHECK(start_callback_.is_null());

  StopModels();

  sync_service_->DeactivateDataType(type());

  if (local_service_.get()) {
    local_service_->StopSyncing(type());
  }

  state_ = NOT_RUNNING;
}

syncable::ModelType UIDataTypeController::type() const {
  DCHECK(syncable::IsRealDataType(type_));
  return type_;
}

void UIDataTypeController::StopModels() {
  // Do nothing by default.
}

browser_sync::ModelSafeGroup UIDataTypeController::model_safe_group() const {
  DCHECK(syncable::IsRealDataType(type_));
  return browser_sync::GROUP_UI;
}

std::string UIDataTypeController::name() const {
  // For logging only.
  return syncable::ModelTypeToString(type());
}

DataTypeController::State UIDataTypeController::state() const {
  return state_;
}

void UIDataTypeController::OnUnrecoverableError(
    const tracked_objects::Location& from_here, const std::string& message) {
  RecordUnrecoverableError(from_here, message);

  // The ProfileSyncService will invoke our Stop() method in response to this.
  // We dont know the current state of the caller. Posting a task will allow
  // the caller to unwind the stack before we process unrecoverable error.
  MessageLoop::current()->PostTask(from_here,
      base::Bind(&ProfileSyncService::OnUnrecoverableError,
                 sync_service_->AsWeakPtr(),
                 from_here,
                 message));
}

void UIDataTypeController::OnSingleDatatypeUnrecoverableError(
    const tracked_objects::Location& from_here, const std::string& message) {
  RecordUnrecoverableError(from_here, message);
  sync_service_->OnDisableDatatype(type(), from_here, message);
}

void UIDataTypeController::RecordAssociationTime(base::TimeDelta time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_TIMES("Sync." type_str "AssociationTime", time);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void UIDataTypeController::RecordStartFailure(StartResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures", type(),
                            syncable::MODEL_TYPE_COUNT);
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_ENUMERATION("Sync." type_str "StartFailure", result, \
                              MAX_START_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

}  // namespace browser_sync
