// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/model_association_manager.h"

#include <algorithm>
#include <functional>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "sync/internal_api/public/base/model_type.h"

using content::BrowserThread;
using syncer::ModelTypeSet;

namespace browser_sync {
// The amount of time we wait for a datatype to load. If the type has
// not finished loading we move on to the next type. Once this type
// finishes loading we will do a configure to associate this type. Note
// that in most cases types finish loading before this timeout.
const int64 kDataTypeLoadWaitTimeInSeconds = 120;
namespace {

static const syncer::ModelType kStartOrder[] = {
  syncer::NIGORI,               //  Listed for completeness.
  syncer::DEVICE_INFO,          //  Listed for completeness.
  syncer::EXPERIMENTS,          //  Listed for completeness.
  syncer::PROXY_TABS,           //  Listed for completeness.
  syncer::BOOKMARKS,            //  UI thread datatypes.
  syncer::MANAGED_USERS,        //  Syncing managed users on initial login might
                                //  block creating a new managed user, so we
                                //  want to do it early.
  syncer::PREFERENCES,
  syncer::PRIORITY_PREFERENCES,
  syncer::EXTENSIONS,
  syncer::APPS,
  syncer::THEMES,
  syncer::SEARCH_ENGINES,
  syncer::SESSIONS,
  syncer::APP_NOTIFICATIONS,
  syncer::DICTIONARY,
  syncer::FAVICON_IMAGES,
  syncer::FAVICON_TRACKING,
  syncer::MANAGED_USER_SETTINGS,
  syncer::AUTOFILL,             // Non-UI thread datatypes.
  syncer::AUTOFILL_PROFILE,
  syncer::EXTENSION_SETTINGS,
  syncer::APP_SETTINGS,
  syncer::TYPED_URLS,
  syncer::PASSWORDS,
  syncer::HISTORY_DELETE_DIRECTIVES,
  syncer::SYNCED_NOTIFICATIONS,
};

COMPILE_ASSERT(arraysize(kStartOrder) ==
               syncer::MODEL_TYPE_COUNT - syncer::FIRST_REAL_MODEL_TYPE,
               kStartOrder_IncorrectSize);

// Comparator used when sorting data type controllers.
class SortComparator : public std::binary_function<DataTypeController*,
                                                   DataTypeController*,
                                                   bool> {
 public:
  explicit SortComparator(std::map<syncer::ModelType, int>* order)
      : order_(order) { }

  // Returns true if lhs precedes rhs.
  bool operator() (DataTypeController* lhs, DataTypeController* rhs) {
    return (*order_)[lhs->type()] < (*order_)[rhs->type()];
  }

 private:
  std::map<syncer::ModelType, int>* order_;
};

syncer::DataTypeAssociationStats BuildAssociationStatsFromMergeResults(
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result,
    const base::TimeDelta& association_wait_time,
    const base::TimeDelta& association_time) {
  DCHECK_EQ(local_merge_result.model_type(), syncer_merge_result.model_type());
  syncer::DataTypeAssociationStats stats;
  stats.had_error = local_merge_result.error().IsSet() ||
                    syncer_merge_result.error().IsSet();
  stats.num_local_items_before_association =
      local_merge_result.num_items_before_association();
  stats.num_sync_items_before_association =
      syncer_merge_result.num_items_before_association();
  stats.num_local_items_after_association =
      local_merge_result.num_items_after_association();
  stats.num_sync_items_after_association =
      syncer_merge_result.num_items_after_association();
  stats.num_local_items_added =
      local_merge_result.num_items_added();
  stats.num_local_items_deleted =
      local_merge_result.num_items_deleted();
  stats.num_local_items_modified =
      local_merge_result.num_items_modified();
  stats.local_version_pre_association =
      local_merge_result.pre_association_version();
  stats.num_sync_items_added =
      syncer_merge_result.num_items_added();
  stats.num_sync_items_deleted =
      syncer_merge_result.num_items_deleted();
  stats.num_sync_items_modified =
      syncer_merge_result.num_items_modified();
  stats.sync_version_pre_association =
      syncer_merge_result.pre_association_version();
  stats.association_wait_time = association_wait_time;
  stats.association_time = association_time;
  return stats;
}

}  // namespace

ModelAssociationManager::ModelAssociationManager(
    const DataTypeController::TypeMap* controllers,
    ModelAssociationResultProcessor* processor)
    : state_(IDLE),
      currently_associating_(NULL),
      controllers_(controllers),
      result_processor_(processor),
      weak_ptr_factory_(this) {

  // Ensure all data type controllers are stopped.
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DCHECK_EQ(DataTypeController::NOT_RUNNING, (*it).second->state());
  }

  // Build a ModelType -> order map for sorting.
  for (int i = 0; i < static_cast<int>(arraysize(kStartOrder)); i++)
    start_order_[kStartOrder[i]] = i;
}

ModelAssociationManager::~ModelAssociationManager() {
}

void ModelAssociationManager::Initialize(syncer::ModelTypeSet desired_types) {
  // TODO(tim): Bug 134550.  CHECKing to ensure no reentrancy on dev channel.
  // Remove this.
  CHECK_EQ(state_, IDLE);
  needs_start_.clear();
  needs_stop_.clear();
  failed_data_types_info_.clear();
  associating_types_.Clear();
  needs_crypto_types_.Clear();
  state_ = INITIALIZED_TO_CONFIGURE;

  DVLOG(1) << "ModelAssociationManager: Initializing for "
           << syncer::ModelTypeSetToString(desired_types);

  // Stop the types that are still loading from the previous configuration.
  // If they are enabled we will start them here once again.
  for (std::vector<DataTypeController*>::const_iterator it =
           pending_model_load_.begin();
       it != pending_model_load_.end();
       ++it) {
    DVLOG(1) << "ModelAssociationManager: Stopping "
             << (*it)->name()
             << " before initialization";
    (*it)->Stop();
  }

  pending_model_load_.clear();
  waiting_to_associate_.clear();
  currently_associating_ = NULL;

  // Add any data type controllers into that needs_stop_ list that are
  // currently MODEL_STARTING, ASSOCIATING, RUNNING or DISABLED.
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DataTypeController* dtc = (*it).second.get();
    if (!desired_types.Has(dtc->type()) &&
        (dtc->state() == DataTypeController::MODEL_STARTING ||
         dtc->state() == DataTypeController::ASSOCIATING ||
         dtc->state() == DataTypeController::RUNNING ||
         dtc->state() == DataTypeController::DISABLED)) {
      needs_stop_.push_back(dtc);
      DVLOG(1) << "ModelTypeToString: Will stop " << dtc->name();
    }
  }
  // Sort these according to kStartOrder.
  std::sort(needs_stop_.begin(),
            needs_stop_.end(),
            SortComparator(&start_order_));
}

void ModelAssociationManager::StartAssociationAsync(
    const syncer::ModelTypeSet& types_to_associate) {
  DCHECK(state_ == INITIALIZED_TO_CONFIGURE || state_ == IDLE);
  state_ = CONFIGURING;

  // Calculate |needs_start_| list.
  associating_types_ = types_to_associate;
  GetControllersNeedingStart(&needs_start_);
  // Sort these according to kStartOrder.
  std::sort(needs_start_.begin(),
            needs_start_.end(),
            SortComparator(&start_order_));

  DVLOG(1) << "ModelAssociationManager: Going to start model association";
  association_start_time_ = base::Time::Now();
  LoadModelForNextType();
}

void ModelAssociationManager::ResetForReconfiguration() {
  state_ = IDLE;
  DVLOG(1) << "ModelAssociationManager: Reseting for reconfiguration";
  needs_start_.clear();
  needs_stop_.clear();
  associating_types_.Clear();
  failed_data_types_info_.clear();
  needs_crypto_types_.Clear();
}

void ModelAssociationManager::StopDisabledTypes() {
  DVLOG(1) << "ModelAssociationManager: Stopping disabled types.";
  // Stop requested data types.
  for (size_t i = 0; i < needs_stop_.size(); ++i) {
    DVLOG(1) << "ModelAssociationManager: Stopping " << needs_stop_[i]->name();
    needs_stop_[i]->Stop();
  }
  needs_stop_.clear();
}

void ModelAssociationManager::Stop() {
  bool need_to_call_model_association_done = false;
  DVLOG(1) << "ModelAssociationManager: Stopping MAM";
  if (state_ == CONFIGURING) {
    DVLOG(1) << "ModelAssociationManager: In the middle of configuration while"
             << " stopping";
    state_ = ABORTED;
    DCHECK(currently_associating_ != NULL ||
           needs_start_.size() > 0 ||
           pending_model_load_.size() > 0 ||
           waiting_to_associate_.size() > 0);

    if (currently_associating_) {
      TRACE_EVENT_ASYNC_END1("sync", "ModelAssociation",
                             currently_associating_,
                             "DataType",
                             ModelTypeToString(currently_associating_->type()));
      DVLOG(1) << "ModelAssociationManager: stopping "
               << currently_associating_->name();
      currently_associating_->Stop();
    } else {
      // DTCs in other lists would be stopped below.
      state_ = IDLE;
    }

    DCHECK_EQ(IDLE, state_);

    // We are in the midle of model association. We need to inform the caller
    // so the caller can send notificationst to PSS layer.
    need_to_call_model_association_done = true;
  }

  // Now continue stopping any types that have already started.
  DCHECK(state_ == IDLE ||
         state_ == INITIALIZED_TO_CONFIGURE);
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DataTypeController* dtc = (*it).second.get();
    if (dtc->state() != DataTypeController::NOT_RUNNING &&
        dtc->state() != DataTypeController::STOPPING) {
      dtc->Stop();
      DVLOG(1) << "ModelAssociationManager: Stopped " << dtc->name();
    }
  }

  DataTypeManager::ConfigureResult result(DataTypeManager::ABORTED,
                                          associating_types_,
                                          failed_data_types_info_,
                                          syncer::ModelTypeSet(),
                                          needs_crypto_types_);
  failed_data_types_info_.clear();
  needs_crypto_types_.Clear();
  if (need_to_call_model_association_done) {
    DVLOG(1) << "ModelAssociationManager: Calling OnModelAssociationDone";
    result_processor_->OnModelAssociationDone(result);
  }
}

bool ModelAssociationManager::GetControllersNeedingStart(
    std::vector<DataTypeController*>* needs_start) {
  DVLOG(1) << "ModelAssociationManager: GetControllersNeedingStart";
  // Add any data type controllers into the needs_start_ list that are
  // currently NOT_RUNNING or STOPPING.
  bool found_any = false;
  for (ModelTypeSet::Iterator it = associating_types_.First();
       it.Good(); it.Inc()) {
    DataTypeController::TypeMap::const_iterator dtc =
        controllers_->find(it.Get());
    if (dtc != controllers_->end() &&
        (dtc->second->state() == DataTypeController::NOT_RUNNING ||
         dtc->second->state() == DataTypeController::STOPPING)) {
      found_any = true;
      if (needs_start)
        needs_start->push_back(dtc->second.get());
      if (dtc->second->state() == DataTypeController::DISABLED) {
        DVLOG(1) << "ModelAssociationManager: Found "\
                << syncer::ModelTypeToString(dtc->second->type())
                 << " in disabled state.";
      }
    }
  }
  return found_any;
}

void ModelAssociationManager::AppendToFailedDatatypesAndLogError(
    DataTypeController::StartResult result,
    const syncer::SyncError& error) {
  failed_data_types_info_[error.model_type()] = error;
  LOG(ERROR) << "Failed to associate models for "
             << syncer::ModelTypeToString(error.model_type());
  UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureFailed",
                            ModelTypeToHistogramInt(error.model_type()),
                            syncer::MODEL_TYPE_COUNT);
}

void ModelAssociationManager::TypeStartCallback(
    DataTypeController::StartResult start_result,
    const syncer::SyncMergeResult& local_merge_result,
    const syncer::SyncMergeResult& syncer_merge_result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT_ASYNC_END1("sync", "ModelAssociation",
                         currently_associating_,
                         "DataType",
                         ModelTypeToString(currently_associating_->type()));

  DVLOG(1) << "ModelAssociationManager: TypeStartCallback";
  if (state_ == ABORTED) {
    // Now that we have finished with the current type we can stop
    // if abort was called.
    DVLOG(1) << "ModelAssociationManager: Doing an early return"
             << " because of abort";
    state_ = IDLE;
    return;
  }

  DCHECK(state_ == CONFIGURING);

  // We are done with this type. Clear it.
  DataTypeController* started_dtc = currently_associating_;
  currently_associating_ = NULL;

  if (start_result == DataTypeController::ASSOCIATION_FAILED) {
    DVLOG(1) << "ModelAssociationManager: Encountered a failed type";
    AppendToFailedDatatypesAndLogError(start_result,
                                       local_merge_result.error());
  } else if (start_result == DataTypeController::NEEDS_CRYPTO) {
    DVLOG(1) << "ModelAssociationManager: Encountered an undecryptable type";
    needs_crypto_types_.Put(started_dtc->type());
  }

  // Track the merge results if we succeeded or an association failure
  // occurred.
  if ((DataTypeController::IsSuccessfulResult(start_result) ||
       start_result == DataTypeController::ASSOCIATION_FAILED) &&
      syncer::ProtocolTypes().Has(local_merge_result.model_type())) {
    base::TimeDelta association_wait_time =
        current_type_association_start_time_ - association_start_time_;
    base::TimeDelta association_time =
        base::Time::Now() - current_type_association_start_time_;
    syncer::DataTypeAssociationStats stats =
        BuildAssociationStatsFromMergeResults(local_merge_result,
                                              syncer_merge_result,
                                              association_wait_time,
                                              association_time);
    result_processor_->OnSingleDataTypeAssociationDone(
        local_merge_result.model_type(), stats);
  }

  // If the type started normally, continue to the next type.
  // If the type is waiting for the cryptographer, continue to the next type.
  // Once the cryptographer is ready, we'll attempt to restart this type.
  // If this type encountered a type specific error continue to the next type.
  if (start_result == DataTypeController::NEEDS_CRYPTO ||
      DataTypeController::IsSuccessfulResult(start_result) ||
      start_result == DataTypeController::ASSOCIATION_FAILED) {

    DVLOG(1) << "ModelAssociationManager: type start callback returned "
             << start_result << " so calling LoadModelForNextType";
    LoadModelForNextType();
    return;
  }

  // Any other result requires reconfiguration. Pass it on through the callback.
  LOG(ERROR) << "Failed to configure " << started_dtc->name();
  DCHECK(local_merge_result.error().IsSet());
  DCHECK_EQ(started_dtc->type(), local_merge_result.error().model_type());
  DataTypeManager::ConfigureStatus configure_status =
      DataTypeManager::ABORTED;
  switch (start_result) {
    case DataTypeController::ABORTED:
      configure_status = DataTypeManager::ABORTED;
      break;
    case DataTypeController::UNRECOVERABLE_ERROR:
      configure_status = DataTypeManager::UNRECOVERABLE_ERROR;
      break;
    default:
      NOTREACHED();
      break;
  }

  std::map<syncer::ModelType, syncer::SyncError> errors;
  errors[local_merge_result.model_type()] = local_merge_result.error();

  // Put our state to idle.
  state_ = IDLE;

  DataTypeManager::ConfigureResult configure_result(configure_status,
                                                    associating_types_,
                                                    errors,
                                                    syncer::ModelTypeSet(),
                                                    needs_crypto_types_);
  result_processor_->OnModelAssociationDone(configure_result);
}

void ModelAssociationManager::LoadModelForNextType() {
  DVLOG(1) << "ModelAssociationManager: LoadModelForNextType";
  if (!needs_start_.empty()) {
    DVLOG(1) << "ModelAssociationManager: Starting " << needs_start_[0]->name();

    DataTypeController* dtc = needs_start_[0];
    needs_start_.erase(needs_start_.begin());
    // Move from |needs_start_| to |pending_model_load_|.
    pending_model_load_.insert(pending_model_load_.begin(), dtc);
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(kDataTypeLoadWaitTimeInSeconds),
                 this,
                 &ModelAssociationManager::LoadModelForNextType);
    dtc->LoadModels(base::Bind(
        &ModelAssociationManager::ModelLoadCallback,
        weak_ptr_factory_.GetWeakPtr()));

    return;
  }

  DVLOG(1) << "ModelAssociationManager: All types have models loaded. "
          << "Moving on to StartAssociatingNextType.";

  // If all controllers have their |LoadModels| invoked then pass onto
  // |StartAssociatingNextType|.
  StartAssociatingNextType();
}

void ModelAssociationManager::ModelLoadCallback(
    syncer::ModelType type, syncer::SyncError error) {
  DVLOG(1) << "ModelAssociationManager: ModelLoadCallback for "
          << syncer::ModelTypeToString(type);
  if (state_ == CONFIGURING) {
    DVLOG(1) << "ModelAssociationManager: ModelLoadCallback while configuring";
    for (std::vector<DataTypeController*>::iterator it =
             pending_model_load_.begin();
          it != pending_model_load_.end();
          ++it) {
      if ((*it)->type() == type) {
        // Each type is given |kDataTypeLoadWaitTimeInSeconds| time to load
        // (as controlled by the timer.). If the type does not load in that
        // time we move on to the next type. However if the type does
        // finish loading in that time we want to stop the timer. We stop
        // the timer, if the type that loaded is the same as the type that
        // we started the timer for(as indicated by the type on the head
        // of the list).
        // Note: Regardless of this timer value the associations will always
        // take place serially. The only thing this timer controls is how serial
        // the model load is. If this timer has a value of zero seconds then
        // the model loads will all be parallel.
        if (it == pending_model_load_.begin()) {
          DVLOG(1) << "ModelAssociationManager: Stopping timer";
          timer_.Stop();
        }
        DataTypeController* dtc = *it;
        pending_model_load_.erase(it);
        if (!error.IsSet()) {
          DVLOG(1) << "ModelAssociationManager:"
                  << " Calling StartAssociatingNextType";
          waiting_to_associate_.push_back(dtc);
          StartAssociatingNextType();
        } else {
          DVLOG(1) << "ModelAssociationManager: Encountered error loading";
          syncer::SyncMergeResult local_merge_result(type);
          local_merge_result.set_error(error);
          TypeStartCallback(DataTypeController::ASSOCIATION_FAILED,
                            local_merge_result,
                            syncer::SyncMergeResult(type));
       }
       return;
      }
    }
    NOTREACHED();
    return;
  } else if (state_ == IDLE) {
    DVLOG(1) << "ModelAssociationManager: Models loaded after configure cycle. "
            << "Informing DTM";
    // This datatype finished loading after the deadline imposed by the
    // originating configuration cycle. Inform the DataTypeManager that the
    // type has loaded, so that association may begin.
    result_processor_->OnTypesLoaded();
  } else {
    // If we're not IDLE or CONFIGURING, we're being invoked as part of an abort
    // process (possibly a reconfiguration, or disabling of a broken data type).
    DVLOG(1) << "ModelAssociationManager: ModelLoadCallback occurred while "
             << "not IDLE or CONFIGURING. Doing nothing.";
  }

}

void ModelAssociationManager::StartAssociatingNextType() {
  DCHECK_EQ(state_, CONFIGURING);
  DCHECK_EQ(currently_associating_, static_cast<DataTypeController*>(NULL));

  DVLOG(1) << "ModelAssociationManager: StartAssociatingNextType";
  if (!waiting_to_associate_.empty()) {
    DVLOG(1) << "ModelAssociationManager: Starting "
            << waiting_to_associate_[0]->name();
    DataTypeController* dtc = waiting_to_associate_[0];
    waiting_to_associate_.erase(waiting_to_associate_.begin());
    currently_associating_ = dtc;
    current_type_association_start_time_ = base::Time::Now();
    TRACE_EVENT_ASYNC_BEGIN1("sync", "ModelAssociation",
                             currently_associating_,
                             "DataType",
                             ModelTypeToString(currently_associating_->type()));
    dtc->StartAssociating(base::Bind(
        &ModelAssociationManager::TypeStartCallback,
        weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // We are done with this cycle of association. Stop any failed types now.
  needs_stop_.clear();
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DataTypeController* dtc = (*it).second.get();
    if (failed_data_types_info_.count(dtc->type()) > 0 &&
        dtc->state() != DataTypeController::NOT_RUNNING) {
      needs_stop_.push_back(dtc);
      DVLOG(1) << "ModelTypeToString: Will stop " << dtc->name();
    }
  }
  StopDisabledTypes();

  state_ = IDLE;

  DataTypeManager::ConfigureStatus configure_status = DataTypeManager::OK;

  if (!failed_data_types_info_.empty() ||
      !GetTypesWaitingToLoad().Empty() ||
      !needs_crypto_types_.Empty()) {
    // We have not configured all types that we have been asked to configure.
    // Either we have failed types or types that have not completed loading
    // yet.
    DVLOG(1) << "ModelAssociationManager: setting partial success";
    configure_status = DataTypeManager::PARTIAL_SUCCESS;
  }

  DataTypeManager::ConfigureResult result(configure_status,
                                          associating_types_,
                                          failed_data_types_info_,
                                          GetTypesWaitingToLoad(),
                                          needs_crypto_types_);
  result_processor_->OnModelAssociationDone(result);
  return;
}

syncer::ModelTypeSet ModelAssociationManager::GetTypesWaitingToLoad() {
  syncer::ModelTypeSet result;
  for (std::vector<DataTypeController*>::const_iterator it =
           pending_model_load_.begin();
       it != pending_model_load_.end();
       ++it) {
    result.Put((*it)->type());
  }
  return result;
}

base::OneShotTimer<ModelAssociationManager>*
    ModelAssociationManager::GetTimerForTesting() {
  return &timer_;
}

}  // namespace browser_sync
