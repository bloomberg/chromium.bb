// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/non_blocking_data_type_manager.h"

#include "base/sequenced_task_runner.h"
#include "components/sync_driver/non_blocking_data_type_controller.h"
#include "sync/internal_api/public/non_blocking_type_processor.h"

namespace browser_sync {

NonBlockingDataTypeManager::NonBlockingDataTypeManager()
    : non_blocking_data_type_controllers_deleter_(
          &non_blocking_data_type_controllers_) {}

NonBlockingDataTypeManager::~NonBlockingDataTypeManager() {}

void NonBlockingDataTypeManager::RegisterType(
    syncer::ModelType type,
    bool enabled) {
  DCHECK_EQ(0U, non_blocking_data_type_controllers_.count(type))
      << "Duplicate registration of type " << ModelTypeToString(type);

  non_blocking_data_type_controllers_.insert(std::make_pair(
      type,
      new NonBlockingDataTypeController(
          type,
          enabled)));
}

void NonBlockingDataTypeManager::InitializeTypeProcessor(
      syncer::ModelType type,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::WeakPtr<syncer::NonBlockingTypeProcessor>& processor) {
  NonBlockingDataTypeControllerMap::iterator it =
      non_blocking_data_type_controllers_.find(type);
  DCHECK(it != non_blocking_data_type_controllers_.end());
  it->second->InitializeProcessor(task_runner, processor);
}

void NonBlockingDataTypeManager::ConnectSyncBackend(
    scoped_ptr<syncer::SyncCoreProxy> proxy) {
  for (NonBlockingDataTypeControllerMap::iterator it =
       non_blocking_data_type_controllers_.begin();
       it != non_blocking_data_type_controllers_.end(); ++it) {
    it->second->InitializeSyncCoreProxy(proxy->Clone());
  }
}

void NonBlockingDataTypeManager::DisconnectSyncBackend() {
  for (NonBlockingDataTypeControllerMap::iterator it =
       non_blocking_data_type_controllers_.begin();
       it != non_blocking_data_type_controllers_.end(); ++it) {
    it->second->ClearSyncCoreProxy();
  }
}

void NonBlockingDataTypeManager::SetPreferredTypes(
    syncer::ModelTypeSet preferred_types) {
  for (NonBlockingDataTypeControllerMap::iterator it =
       non_blocking_data_type_controllers_.begin();
       it != non_blocking_data_type_controllers_.end(); ++it) {
    it->second->SetIsPreferred(preferred_types.Has(it->first));
  }
}

syncer::ModelTypeSet NonBlockingDataTypeManager::GetRegisteredTypes() const {
  syncer::ModelTypeSet result;
  for (NonBlockingDataTypeControllerMap::const_iterator it =
       non_blocking_data_type_controllers_.begin();
       it != non_blocking_data_type_controllers_.end(); ++it) {
    result.Put(it->first);
  }
  return result;
}

}  // namespace browser_sync
