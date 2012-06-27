// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <functional>

#include "base/debug/trace_event.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"

#include "chrome/browser/sync/glue/model_association_manager.h"

#include "content/public/browser/browser_thread.h"
#include <algorithm>
#include <functional>

#include "base/debug/trace_event.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"


using content::BrowserThread;
using syncable::ModelTypeSet;

namespace browser_sync {
// The amount of time we wait for a datatype to load. If the type has
// not finished loading we move on to the next type. Once this type
// finishes loading we will do a configure to associate this type. Note
// that in most cases types finish loading before this timeout.
const int64 kDataTypeLoadWaitTimeInSeconds = 120;
namespace {

static const syncable::ModelType kStartOrder[] = {
  syncable::NIGORI,               //  Listed for completeness.
  syncable::BOOKMARKS,            //  UI thread datatypes.
  syncable::PREFERENCES,
  syncable::EXTENSIONS,
  syncable::APPS,
  syncable::THEMES,
  syncable::SEARCH_ENGINES,
  syncable::SESSIONS,
  syncable::APP_NOTIFICATIONS,
  syncable::AUTOFILL,             // Non-UI thread datatypes.
  syncable::AUTOFILL_PROFILE,
  syncable::EXTENSION_SETTINGS,
  syncable::APP_SETTINGS,
  syncable::TYPED_URLS,
  syncable::PASSWORDS,
};

COMPILE_ASSERT(arraysize(kStartOrder) ==
               syncable::MODEL_TYPE_COUNT - syncable::FIRST_REAL_MODEL_TYPE,
               kStartOrder_IncorrectSize);

// Comparator used when sorting data type controllers.
class SortComparator : public std::binary_function<DataTypeController*,
                                                   DataTypeController*,
                                                   bool> {
 public:
  explicit SortComparator(std::map<syncable::ModelType, int>* order)
      : order_(order) { }

  // Returns true if lhs precedes rhs.
  bool operator() (DataTypeController* lhs, DataTypeController* rhs) {
    return (*order_)[lhs->type()] < (*order_)[rhs->type()];
  }

 private:
  std::map<syncable::ModelType, int>* order_;
};

}  // namespace

ModelAssociationManager::ModelAssociationManager(
    const DataTypeController::TypeMap* controllers,
    ModelAssociationResultProcessor* processor)
    : state_(IDLE),
      currently_associating_(NULL),
      controllers_(controllers),
      result_processor_(processor),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {

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

void ModelAssociationManager::Initialize(
    syncable::ModelTypeSet desired_types) {
  DCHECK_EQ(state_, IDLE);
  needs_start_.clear();
  needs_stop_.clear();
  failed_datatypes_info_.clear();
  desired_types_ = desired_types;
  state_ = INITIAILIZED_TO_CONFIGURE;

  DVLOG(1) << "ModelAssociationManager: Initializing";

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

  // We need to calculate our |needs_start_| and |needs_stop_| list.
  GetControllersNeedingStart(&needs_start_);
  // Sort these according to kStartOrder.
  std::sort(needs_start_.begin(),
            needs_start_.end(),
            SortComparator(&start_order_));

  // Add any data type controllers into that needs_stop_ list that are
  // currently MODEL_STARTING, ASSOCIATING, RUNNING or DISABLED.
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DataTypeController* dtc = (*it).second;
    if (!desired_types.Has(dtc->type()) && (
            dtc->state() == DataTypeController::MODEL_STARTING ||
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

void ModelAssociationManager::StartAssociationAsync() {
  DCHECK_EQ(state_, INITIAILIZED_TO_CONFIGURE);
  state_ = CONFIGURING;
  DVLOG(1) << "ModelAssociationManager: Going to start model association";
  LoadModelForNextType();
}

void ModelAssociationManager::ResetForReconfiguration() {
  DCHECK_EQ(state_, INITIAILIZED_TO_CONFIGURE);
  state_ = IDLE;
  DVLOG(1) << "ModelAssociationManager: Reseting for reconfiguration";
  needs_start_.clear();
  needs_stop_.clear();
  failed_datatypes_info_.clear();
}

void ModelAssociationManager::StopDisabledTypes() {
  DCHECK_EQ(state_, INITIAILIZED_TO_CONFIGURE);
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
    DVLOG(1) << "ModelAssociationManager: In the middle of configuratio while"
             << " stopping";
    state_ = ABORTED;
    DCHECK(currently_associating_ != NULL ||
           needs_start_.size() > 0 ||
           pending_model_load_.size() > 0 ||
           waiting_to_associate_.size() > 0);

    if (currently_associating_) {
      TRACE_EVENT_END0("sync", "ModelAssociation");
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
         state_ == INITIAILIZED_TO_CONFIGURE);
  for (DataTypeController::TypeMap::const_iterator it = controllers_->begin();
       it != controllers_->end(); ++it) {
    DataTypeController* dtc = (*it).second;
    if (dtc->state() != DataTypeController::NOT_RUNNING &&
        dtc->state() != DataTypeController::STOPPING) {
      dtc->Stop();
      DVLOG(1) << "ModelAssociationManager: Stopped " << dtc->name();
    }
  }

  if (need_to_call_model_association_done) {
    DVLOG(1) << "ModelAssociationManager: Calling OnModelAssociationDone";
    DataTypeManager::ConfigureResult result(DataTypeManager::ABORTED,
                                            desired_types_,
                                            failed_datatypes_info_,
                                            syncable::ModelTypeSet());
    result_processor_->OnModelAssociationDone(result);
  }

  failed_datatypes_info_.clear();
}

bool ModelAssociationManager::GetControllersNeedingStart(
    std::vector<DataTypeController*>* needs_start) {
  DVLOG(1) << "ModelAssociationManager: GetControllersNeedingStart";
  // Add any data type controllers into the needs_start_ list that are
  // currently NOT_RUNNING or STOPPING.
  bool found_any = false;
  for (ModelTypeSet::Iterator it = desired_types_.First();
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
                << syncable::ModelTypeToString(dtc->second->type())
                 << " in disabled state.";
      }
    }
  }
  return found_any;
}

void ModelAssociationManager::AppendToFailedDatatypesAndLogError(
    DataTypeController::StartResult result,
    const csync::SyncError& error) {
  failed_datatypes_info_.push_back(error);
  LOG(ERROR) << "Failed to associate models for "
             << syncable::ModelTypeToString(error.type());
  UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureFailed",
                            error.type(),
                            syncable::MODEL_TYPE_COUNT);
}

void ModelAssociationManager::TypeStartCallback(
    DataTypeController::StartResult result,
    const csync::SyncError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TRACE_EVENT_END0("sync", "ModelAssociation");

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

  if (result == DataTypeController::ASSOCIATION_FAILED) {
    DVLOG(1) << "ModelAssociationManager: Encountered a failed type";
    AppendToFailedDatatypesAndLogError(result, error);
  }

  // If the type started normally, continue to the next type.
  // If the type is waiting for the cryptographer, continue to the next type.
  // Once the cryptographer is ready, we'll attempt to restart this type.
  // If this type encountered a type specific error continue to the next type.
  if (result == DataTypeController::NEEDS_CRYPTO ||
      result == DataTypeController::OK ||
      result == DataTypeController::OK_FIRST_RUN ||
      result == DataTypeController::ASSOCIATION_FAILED) {
    DVLOG(1) << "ModelAssociationManager: type start callback returned "
             << result << " so calling LoadModelForNextType";
    LoadModelForNextType();
    return;
  }

  // Any other result requires reconfiguration. Pass it on through the callback.
  LOG(ERROR) << "Failed to configure " << started_dtc->name();
  DCHECK(error.IsSet());
  DCHECK_EQ(started_dtc->type(), error.type());
  DataTypeManager::ConfigureStatus configure_status =
      DataTypeManager::ABORTED;
  switch (result) {
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

  std::list<csync::SyncError> errors;
  errors.push_back(error);

  // Put our state to idle.
  state_ = IDLE;

  DataTypeManager::ConfigureResult configure_result(configure_status,
                                                    desired_types_,
                                                    errors,
                                                    syncable::ModelTypeSet());
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

  DVLOG(1) << "ModelAssociationManager: All types have models loaded."
          << "Moving on to StartAssociatingNextType.";

  // If all controllers have their |LoadModels| invoked then pass onto
  // |StartAssociatingNextType|.
  StartAssociatingNextType();
}

void ModelAssociationManager::ModelLoadCallback(
    syncable::ModelType type, csync::SyncError error) {
  DVLOG(1) << "ModelAssociationManager: ModelLoadCallback for "
          << syncable::ModelTypeToString(type);
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
         // Treat it like a regular error.
         AppendToFailedDatatypesAndLogError(
             DataTypeController::ASSOCIATION_FAILED,
             error);
       }
       return;
      }
    }
    NOTREACHED();
    return;
  } else {
    DVLOG(1) << "ModelAssociationManager: Models loaded after configure cycle"
            << "Informing DTM";
    // This datatype finished loading after the deadline imposed by the
    // originating configuration cycle. Inform the DataTypeManager that the
    // type has loaded, so that association may begin.
    result_processor_->OnTypesLoaded();
  }

}

void ModelAssociationManager::StartAssociatingNextType() {
  DCHECK_EQ(state_, CONFIGURING);
  DCHECK_EQ(currently_associating_, static_cast<DataTypeController*>(NULL));

  DVLOG(1) << "ModelAssociationManager: StartAssociatingNextType";
  if (!waiting_to_associate_.empty()) {
    DVLOG(1) << "ModelAssociationManager: Starting "
            << waiting_to_associate_[0]->name();
    TRACE_EVENT_BEGIN1("sync", "ModelAssociation",
                       "DataType",
                       ModelTypeToString(waiting_to_associate_[0]->type()));
    DataTypeController* dtc = waiting_to_associate_[0];
    waiting_to_associate_.erase(waiting_to_associate_.begin());
    currently_associating_ = dtc;
    dtc->StartAssociating(base::Bind(
        &ModelAssociationManager::TypeStartCallback,
        weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // We are done with this cycle of association.
  state_ = IDLE;
  // Do a fresh calculation to see if controllers need starting to account for
  // things like encryption, which may still need to be sorted out before we
  // can announce we're "Done" configuration entirely.
  if (GetControllersNeedingStart(NULL)) {
    DVLOG(1) << "ModelAssociationManager: GetControllersNeedingStart"
             << " returned true. Blocking DataTypeManager";

    DataTypeManager::ConfigureResult configure_result(
        DataTypeManager::CONFIGURE_BLOCKED,
        desired_types_,
        failed_datatypes_info_,
        syncable::ModelTypeSet());
    state_ = IDLE;
    result_processor_->OnModelAssociationDone(configure_result);
    return;
  }

  DataTypeManager::ConfigureStatus configure_status = DataTypeManager::OK;

  if (!failed_datatypes_info_.empty() ||
      !GetTypesWaitingToLoad().Empty()) {
    // We have not configured all types that we have been asked to configure.
    // Either we have failed types or types that have not completed loading
    // yet.
    DVLOG(1) << "ModelAssociationManager: setting partial success";
    configure_status = DataTypeManager::PARTIAL_SUCCESS;
  }

  DataTypeManager::ConfigureResult result(configure_status,
                                          desired_types_,
                                          failed_datatypes_info_,
                                          GetTypesWaitingToLoad());
  result_processor_->OnModelAssociationDone(result);
  return;
}

syncable::ModelTypeSet ModelAssociationManager::GetTypesWaitingToLoad() {
  syncable::ModelTypeSet result;
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

