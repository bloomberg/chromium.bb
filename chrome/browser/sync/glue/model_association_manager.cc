// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
namespace {

static const syncable::ModelType kStartOrder[] = {
  syncable::NIGORI,  //  Listed for completeness.
  syncable::BOOKMARKS,
  syncable::PREFERENCES,
  syncable::AUTOFILL,
  syncable::AUTOFILL_PROFILE,
  syncable::EXTENSION_SETTINGS,
  syncable::EXTENSIONS,
  syncable::APP_SETTINGS,
  syncable::APPS,
  syncable::THEMES,
  syncable::TYPED_URLS,
  syncable::PASSWORDS,
  syncable::SEARCH_ENGINES,
  syncable::SESSIONS,
  syncable::APP_NOTIFICATIONS,
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
      DVLOG(1) << "Will stop " << dtc->name();
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
  LoadModelForNextType();
}

void ModelAssociationManager::ResetForReconfiguration() {
  DCHECK_EQ(state_, INITIAILIZED_TO_CONFIGURE);
  state_ = IDLE;
  needs_start_.clear();
  needs_stop_.clear();
  failed_datatypes_info_.clear();
}

void ModelAssociationManager::StopDisabledTypes() {
  DCHECK_EQ(state_, INITIAILIZED_TO_CONFIGURE);
  // Stop requested data types.
  for (size_t i = 0; i < needs_stop_.size(); ++i) {
    DVLOG(1) << "Stopping " << needs_stop_[i]->name();
    needs_stop_[i]->Stop();
  }
  needs_stop_.clear();
}

void ModelAssociationManager::Stop() {
  bool need_to_call_model_association_done = false;
  if (state_ == CONFIGURING) {
    state_ = ABORTED;
    DCHECK(currently_associating_ != NULL ||
           needs_start_.size() > 0 ||
           pending_model_load_.size() > 0 ||
           waiting_to_associate_.size() > 0);

    if (currently_associating_) {
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
      DVLOG(1) << "Stopped " << dtc->name();
    }
  }

  if (need_to_call_model_association_done) {
    DataTypeManager::ConfigureResult result(DataTypeManager::ABORTED,
                                            desired_types_,
                                            failed_datatypes_info_);
    result_processor_->OnModelAssociationDone(result);
  }

  failed_datatypes_info_.clear();
}

bool ModelAssociationManager::GetControllersNeedingStart(
    std::vector<DataTypeController*>* needs_start) {
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
        DVLOG(1) << "Found " << syncable::ModelTypeToString(dtc->second->type())
                 << " in disabled state.";
      }
    }
  }
  return found_any;
}

void ModelAssociationManager::TypeStartCallback(
    DataTypeController::StartResult result,
    const SyncError& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (state_ == ABORTED) {
    // Now that we have finished with the current type we can stop
    // if abort was called.
    state_ = IDLE;
    return;
  }

  DCHECK(state_ == CONFIGURING);

  // We are done with this type. Clear it.
  DataTypeController* started_dtc = currently_associating_;
  currently_associating_ = NULL;

  if (result == DataTypeController::ASSOCIATION_FAILED) {
    failed_datatypes_info_.push_back(error);
    LOG(ERROR) << "Failed to associate models for "
               << syncable::ModelTypeToString(error.type());
    UMA_HISTOGRAM_ENUMERATION("Sync.ConfigureFailed",
                              error.type(),
                              syncable::MODEL_TYPE_COUNT);
  }

  // If the type started normally, continue to the next type.
  // If the type is waiting for the cryptographer, continue to the next type.
  // Once the cryptographer is ready, we'll attempt to restart this type.
  // If this type encountered a type specific error continue to the next type.
  if (result == DataTypeController::NEEDS_CRYPTO ||
      result == DataTypeController::OK ||
      result == DataTypeController::OK_FIRST_RUN ||
      result == DataTypeController::ASSOCIATION_FAILED) {
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

  std::list<SyncError> errors;
  errors.push_back(error);

  // Put our state to idle.
  state_ = IDLE;

  DataTypeManager::ConfigureResult configure_result(configure_status,
                                                    desired_types_,
                                                    errors);
  result_processor_->OnModelAssociationDone(configure_result);
}

void ModelAssociationManager::LoadModelForNextType() {
  if (!needs_start_.empty()) {
    DVLOG(1) << "Starting " << needs_start_[0]->name();

    DataTypeController* dtc = needs_start_[0];
    needs_start_.erase(needs_start_.begin());
    // Move from |needs_start_| to |pending_model_load_|.
    pending_model_load_.push_back(dtc);
    dtc->LoadModels(base::Bind(
        &ModelAssociationManager::ModelLoadCallback,
        weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If all controllers have their |LoadModels| invoked then pass onto
  // |StartAssociatingNextType|.
  StartAssociatingNextType();
}

void ModelAssociationManager::ModelLoadCallback(
    syncable::ModelType type, SyncError error) {
  DCHECK_EQ(state_, CONFIGURING);

  for (std::vector<DataTypeController*>::iterator it =
           pending_model_load_.begin();
        it != pending_model_load_.end();
        ++it) {
    if ((*it)->type() == type) {
      DataTypeController* dtc = *it;
      pending_model_load_.erase(it);
      if (!error.IsSet()) {
        waiting_to_associate_.push_back(dtc);
        StartAssociatingNextType();
        } else {
        // Treat it like a regular error.
        DCHECK(currently_associating_ == NULL);
        currently_associating_ = dtc;
        TypeStartCallback(DataTypeController::ASSOCIATION_FAILED, error);
      }
     return;
    }
  }

  NOTREACHED();
}


void ModelAssociationManager::StartAssociatingNextType() {
  DCHECK_EQ(state_, CONFIGURING);
  DCHECK_EQ(currently_associating_, static_cast<DataTypeController*>(NULL));
  if (!waiting_to_associate_.empty()) {
    DVLOG(1) << "Starting " << waiting_to_associate_[0]->name();
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
    DVLOG(1) << "GetControllersNeedingStart returned true."
             << " Blocking DataTypeManager";

    DataTypeManager::ConfigureResult configure_result(
        DataTypeManager::CONFIGURE_BLOCKED,
        desired_types_,
        failed_datatypes_info_);
    state_ = IDLE;
    result_processor_->OnModelAssociationDone(configure_result);
    return;
  }

  DataTypeManager::ConfigureStatus configure_status = DataTypeManager::OK;
  if (!failed_datatypes_info_.empty())
    configure_status = DataTypeManager::PARTIAL_SUCCESS;

  DataTypeManager::ConfigureResult result(configure_status,
                                          desired_types_,
                                          failed_datatypes_info_);
  result_processor_->OnModelAssociationDone(result);
  return;
}

}  // namespace browser_sync

