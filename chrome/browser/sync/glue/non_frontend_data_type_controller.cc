// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/chrome_report_unrecoverable_error.h"
#include "chrome/browser/sync/profile_sync_components_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "components/sync_driver/change_processor.h"
#include "components/sync_driver/model_associator.h"
#include "content/public/browser/browser_thread.h"
#include "sync/api/sync_error.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/util/weak_handle.h"
#include "sync/util/data_type_histogram.h"

using content::BrowserThread;

namespace browser_sync {

class NonFrontendDataTypeController::BackendComponentsContainer {
 public:
  explicit BackendComponentsContainer(
      NonFrontendDataTypeController* controller);
  ~BackendComponentsContainer();
  void Run();
  void Disconnect();

 private:
  bool CreateComponents();
  void Associate();

  // For creating components.
  NonFrontendDataTypeController* controller_;
  base::Lock controller_lock_;

  syncer::ModelType type_;

  // For returning association results to controller on UI.
  syncer::WeakHandle<NonFrontendDataTypeController> controller_handle_;

  scoped_ptr<sync_driver::AssociatorInterface> model_associator_;
  scoped_ptr<sync_driver::ChangeProcessor> change_processor_;
};

NonFrontendDataTypeController::
BackendComponentsContainer::BackendComponentsContainer(
    NonFrontendDataTypeController* controller)
    : controller_(controller),
      type_(controller->type()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  controller_handle_ =
      syncer::MakeWeakHandle(controller_->weak_ptr_factory_.GetWeakPtr());
}

NonFrontendDataTypeController::
BackendComponentsContainer::~BackendComponentsContainer() {
  if (model_associator_)
    model_associator_->DisassociateModels();
}

void NonFrontendDataTypeController::BackendComponentsContainer::Run() {
  DCHECK(controller_->IsOnBackendThread());
  if (CreateComponents())
    Associate();
}

bool
NonFrontendDataTypeController::BackendComponentsContainer::CreateComponents() {
  base::AutoLock al(controller_lock_);
  if (!controller_) {
    DVLOG(1) << "Controller was stopped before sync components are created.";
    return false;
  }

  ProfileSyncComponentsFactory::SyncComponents sync_components =
      controller_->CreateSyncComponents();
  model_associator_.reset(sync_components.model_associator);
  change_processor_.reset(sync_components.change_processor);
  return true;
}

void NonFrontendDataTypeController::BackendComponentsContainer::Associate() {
  CHECK(model_associator_);

  bool succeeded = false;

  browser_sync::NonFrontendDataTypeController::AssociationResult result(type_);
  if (!model_associator_->CryptoReadyIfNecessary()) {
    result.needs_crypto = true;
  } else {
    base::TimeTicks start_time = base::TimeTicks::Now();

    if (!model_associator_->SyncModelHasUserCreatedNodes(
        &result.sync_has_nodes)) {
      result.unrecoverable_error = true;
      result.error = syncer::SyncError(FROM_HERE,
                                       syncer::SyncError::UNRECOVERABLE_ERROR,
                                       "Failed to load sync nodes",
                                       type_);
    } else {
      result.error = model_associator_->AssociateModels(
          &result.local_merge_result, &result.syncer_merge_result);

      // Return components to frontend when no error.
      if (!result.error.IsSet()) {
        succeeded = true;
        result.change_processor = change_processor_.get();
        result.model_associator = model_associator_.get();
      }
    }
    result.association_time = base::TimeTicks::Now() - start_time;
  }
  result.local_merge_result.set_error(result.error);

  // Destroy processor/associator on backend on failure.
  if (!succeeded) {
    base::AutoLock al(controller_lock_);
    model_associator_->DisassociateModels();
    change_processor_.reset();
    model_associator_.reset();
  }

  controller_handle_.Call(
      FROM_HERE,
      &browser_sync::NonFrontendDataTypeController::AssociationCallback,
      result);
}

void NonFrontendDataTypeController::BackendComponentsContainer::Disconnect() {
  base::AutoLock al(controller_lock_);
  CHECK(controller_);

  if (change_processor_)
    controller_->DisconnectProcessor(change_processor_.get());
  if (model_associator_)
    model_associator_->AbortAssociation();

  controller_ = NULL;
}

NonFrontendDataTypeController::AssociationResult::AssociationResult(
    syncer::ModelType type)
    : needs_crypto(false),
      unrecoverable_error(false),
      sync_has_nodes(false),
      local_merge_result(type),
      syncer_merge_result(type),
      change_processor(NULL),
      model_associator(NULL) {}

NonFrontendDataTypeController::AssociationResult::~AssociationResult() {}

// TODO(tim): Legacy controllers are being left behind in componentization
// effort for now, hence  still having a dependency on ProfileSyncService.
// That dep can probably be removed without too much work.
NonFrontendDataTypeController::NonFrontendDataTypeController(
    scoped_refptr<base::MessageLoopProxy> ui_thread,
    const base::Closure& error_callback,
    ProfileSyncComponentsFactory* profile_sync_factory,
    Profile* profile,
    ProfileSyncService* sync_service)
    : DataTypeController(ui_thread, error_callback),
      state_(NOT_RUNNING),
      profile_sync_factory_(profile_sync_factory),
      profile_(profile),
      profile_sync_service_(sync_service),
      model_associator_(NULL),
      change_processor_(NULL),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_sync_factory_);
  DCHECK(profile_);
  DCHECK(profile_sync_service_);
}

void NonFrontendDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!model_load_callback.is_null());
  if (state_ != NOT_RUNNING) {
    model_load_callback.Run(type(),
                            syncer::SyncError(FROM_HERE,
                                              syncer::SyncError::DATATYPE_ERROR,
                                              "Model already loaded",
                                              type()));
    return;
  }

  state_ = MODEL_STARTING;
  if (!StartModels()) {
    // We failed to start the models. There is no point in waiting.
    // Note: This code is deprecated. The only 2 datatypes here,
    // passwords and typed urls, dont have any special loading. So if we
    // get a false it means they failed.
    DCHECK(state_ == NOT_RUNNING || state_ == MODEL_STARTING
           || state_ == DISABLED);
    model_load_callback.Run(type(),
                            syncer::SyncError(FROM_HERE,
                                              syncer::SyncError::DATATYPE_ERROR,
                                              "Failed loading",
                                              type()));
    return;
  }
  state_ = MODEL_LOADED;

  model_load_callback.Run(type(), syncer::SyncError());
}

void NonFrontendDataTypeController::OnModelLoaded() {
  NOTREACHED();
}

void NonFrontendDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!start_callback.is_null());
  DCHECK(!components_container_);
  DCHECK_EQ(state_, MODEL_LOADED);

  // Kick off association on the thread the datatype resides on.
  state_ = ASSOCIATING;
  start_callback_ = start_callback;

  components_container_.reset(new BackendComponentsContainer(this));

  if (!PostTaskOnBackendThread(
      FROM_HERE,
      base::Bind(&BackendComponentsContainer::Run,
                 base::Unretained(components_container_.get())))) {
    syncer::SyncError error(
        FROM_HERE,
        syncer::SyncError::DATATYPE_ERROR,
        "Failed to post StartAssociation", type());
    syncer::SyncMergeResult local_merge_result(type());
    local_merge_result.set_error(error);
    StartDone(ASSOCIATION_FAILED,
              local_merge_result,
              syncer::SyncMergeResult(type()));
  }
}

void DestroyComponentsInBackend(
    NonFrontendDataTypeController::BackendComponentsContainer *container) {
  delete container;
}

void NonFrontendDataTypeController::Stop() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (state_ == NOT_RUNNING)
    return;

  // Deactivate the date type on the UI thread first to stop processing
  // sync server changes. This needs to happen before posting task to destroy
  // processor and associator on backend. Otherwise it could crash if syncer
  // post work to backend after destruction task and that work is run before
  // deactivation.
  profile_sync_service()->DeactivateDataType(type());

  // Ignore association callback.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Disconnect on UI and post task to destroy on backend.
  if (components_container_) {
    components_container_->Disconnect();
    PostTaskOnBackendThread(
          FROM_HERE,
          base::Bind(&DestroyComponentsInBackend,
                     components_container_.release()));
    model_associator_ = NULL;
    change_processor_ = NULL;
  }

  // Call start callback if waiting for association.
  if (state_ == ASSOCIATING) {
    StartDone(ABORTED,
              syncer::SyncMergeResult(type()),
              syncer::SyncMergeResult(type()));

  }

  state_ = NOT_RUNNING;
}

std::string NonFrontendDataTypeController::name() const {
  // For logging only.
  return syncer::ModelTypeToString(type());
}

sync_driver::DataTypeController::State NonFrontendDataTypeController::state()
    const {
  return state_;
}

void NonFrontendDataTypeController::OnSingleDataTypeUnrecoverableError(
    const syncer::SyncError& error) {
  DCHECK(IsOnBackendThread());
  DCHECK_EQ(type(), error.model_type());
  RecordUnrecoverableError(error.location(), error.message());
  BrowserThread::PostTask(BrowserThread::UI, error.location(),
      base::Bind(&NonFrontendDataTypeController::DisableImpl,
                 this,
                 error));
}

NonFrontendDataTypeController::NonFrontendDataTypeController()
    : DataTypeController(base::MessageLoopProxy::current(), base::Closure()),
      state_(NOT_RUNNING),
      profile_sync_factory_(NULL),
      profile_(NULL),
      profile_sync_service_(NULL),
      model_associator_(NULL),
      change_processor_(NULL),
      weak_ptr_factory_(this) {
}

NonFrontendDataTypeController::~NonFrontendDataTypeController() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!change_processor_);
  DCHECK(!model_associator_);
}

bool NonFrontendDataTypeController::StartModels() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(state_, MODEL_STARTING);
  // By default, no additional services need to be started before we can proceed
  // with model association, so do nothing.
  return true;
}

bool NonFrontendDataTypeController::IsOnBackendThread() {
  return !BrowserThread::CurrentlyOn(BrowserThread::UI);
}

void NonFrontendDataTypeController::StartDone(
    DataTypeController::ConfigureResult start_result,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DataTypeController::State new_state;

  if (IsSuccessfulResult(start_result)) {
    new_state = RUNNING;
  } else {
    new_state = (start_result == ASSOCIATION_FAILED ? DISABLED : NOT_RUNNING);
    if (IsUnrecoverableResult(start_result))
      RecordUnrecoverableError(FROM_HERE, "StartFailed");
  }

  StartDoneImpl(start_result, new_state, local_merge_result,
                syncer_merge_result);
}

void NonFrontendDataTypeController::StartDoneImpl(
    DataTypeController::ConfigureResult start_result,
    DataTypeController::State new_state,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  state_ = new_state;
  if (state_ != RUNNING) {
    // Start failed.
    RecordStartFailure(start_result);
  }

  start_callback_.Run(start_result, local_merge_result, syncer_merge_result);
}

void NonFrontendDataTypeController::DisableImpl(
    const syncer::SyncError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!start_callback_.is_null()) {
    syncer::SyncMergeResult local_merge_result(type());
    local_merge_result.set_error(error);
    start_callback_.Run(RUNTIME_ERROR,
                        local_merge_result,
                        syncer::SyncMergeResult(type()));
  }
}

void NonFrontendDataTypeController::RecordAssociationTime(
    base::TimeDelta time) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_TIMES("Sync." type_str "AssociationTime", time);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void NonFrontendDataTypeController::RecordStartFailure(ConfigureResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeStartFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);
#define PER_DATA_TYPE_MACRO(type_str) \
    UMA_HISTOGRAM_ENUMERATION("Sync." type_str "StartFailure", result, \
                              MAX_START_RESULT);
  SYNC_DATA_TYPE_HISTOGRAM(type());
#undef PER_DATA_TYPE_MACRO
}

void NonFrontendDataTypeController::RecordUnrecoverableError(
    const tracked_objects::Location& from_here,
    const std::string& message) {
  DVLOG(1) << "Datatype Controller failed for type "
           << ModelTypeToString(type()) << "  "
           << message << " at location "
           << from_here.ToString();
  UMA_HISTOGRAM_ENUMERATION("Sync.DataTypeRunFailures",
                            ModelTypeToHistogramInt(type()),
                            syncer::MODEL_TYPE_COUNT);

  if (!error_callback_.is_null())
    error_callback_.Run();
}


ProfileSyncComponentsFactory*
    NonFrontendDataTypeController::profile_sync_factory() const {
  return profile_sync_factory_;
}

Profile* NonFrontendDataTypeController::profile() const {
  return profile_;
}

ProfileSyncService* NonFrontendDataTypeController::profile_sync_service()
    const {
  return profile_sync_service_;
}

void NonFrontendDataTypeController::set_start_callback(
    const StartCallback& callback) {
  start_callback_ = callback;
}

void NonFrontendDataTypeController::set_state(State state) {
  state_ = state;
}

sync_driver::AssociatorInterface* NonFrontendDataTypeController::associator()
    const {
  return model_associator_;
}

sync_driver::ChangeProcessor*
NonFrontendDataTypeController::GetChangeProcessor() const {
  return change_processor_;
}

void NonFrontendDataTypeController::AssociationCallback(
    AssociationResult result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (result.needs_crypto) {
    StartDone(NEEDS_CRYPTO,
              result.local_merge_result,
              result.syncer_merge_result);
    return;
  }

  if (result.unrecoverable_error) {
    StartDone(UNRECOVERABLE_ERROR,
              result.local_merge_result,
              result.syncer_merge_result);
    return;
  }

  RecordAssociationTime(result.association_time);
  if (result.error.IsSet()) {
    StartDone(ASSOCIATION_FAILED,
              result.local_merge_result,
              result.syncer_merge_result);
    return;
  }

  CHECK(result.change_processor);
  CHECK(result.model_associator);
  change_processor_ = result.change_processor;
  model_associator_ = result.model_associator;

  StartDone(!result.sync_has_nodes ? OK_FIRST_RUN : OK,
            result.local_merge_result,
            result.syncer_merge_result);
}

}  // namespace browser_sync
