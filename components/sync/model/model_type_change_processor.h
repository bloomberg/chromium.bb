// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/location.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/activation_context.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/model_error.h"

namespace syncer {

class MetadataBatch;
class MetadataChangeList;
class ModelTypeSyncBridge;

// Interface used by the ModelTypeSyncBridge to inform sync of local changes.
class ModelTypeChangeProcessor {
 public:
  using StartCallback =
      base::Callback<void(std::unique_ptr<ActivationContext>)>;

  // A factory function to make an implementation of ModelTypeChangeProcessor.
  static std::unique_ptr<ModelTypeChangeProcessor> Create(
      const base::RepeatingClosure& dump_stack,
      ModelType type,
      ModelTypeSyncBridge* bridge);

  ModelTypeChangeProcessor();
  virtual ~ModelTypeChangeProcessor();

  // Inform the processor of a new or updated entity. The |entity_data| param
  // does not need to be fully set, but it should at least have specifics and
  // non-unique name. The processor will fill in the rest if the bridge does
  // not have a reason to care.
  virtual void Put(const std::string& storage_key,
                   std::unique_ptr<EntityData> entity_data,
                   MetadataChangeList* metadata_change_list) = 0;

  // Inform the processor of a deleted entity.
  virtual void Delete(const std::string& storage_key,
                      MetadataChangeList* metadata_change_list) = 0;

  // Sets storage key for the new entity. This function only applies to
  // datatypes that can't generate storage key based on EntityData. Bridge
  // should call this function when handling MergeSyncData/ApplySyncChanges to
  // inform the processor about |storage_key| of an entity identified by
  // |entity_data|. Metadata changes about new entity will be appended to
  // |metadata_change_list|.
  virtual void UpdateStorageKey(const EntityData& entity_data,
                                const std::string& storage_key,
                                MetadataChangeList* metadata_change_list) = 0;

  // Remove entity metadata and do not track the entity. This function only
  // applies to datatypes that can't generate storage key based on EntityData.
  // Bridge should call this function when handling
  // MergeSyncData/ApplySyncChanges to inform the processor that this entity
  // should not been tracked. Datatypes that support GetStorageKey should call
  // change_processor()->Delete() instead.
  virtual void UntrackEntity(const EntityData& entity_data) = 0;

  // The bridge is expected to call this exactly once unless it encounters an
  // error. Ideally ModelReadyToSync() is called as soon as possible during
  // initialization, and must be called before invoking either Put() or
  // Delete(). The bridge needs to be able to synchronously handle
  // MergeSyncData() and ApplySyncChanges() after calling ModelReadyToSync(). If
  // an error is encountered, calling ReportError() instead is sufficient.
  virtual void ModelReadyToSync(std::unique_ptr<MetadataBatch> batch) = 0;

  // Indicates that sync wants to connect a sync worker to this processor. Once
  // the processor has metadata from the bridge, it will pass the info needed
  // for the worker into |callback|. |error_handler| is how the processor will
  // inform sync of any unrecoverable errors, and is guaranteed to outlive the
  // processor. If an error is encountered, |error_handler| should be called
  // and |callback| should not.
  virtual void OnSyncStarting(const ModelErrorHandler& error_handler,
                              const StartCallback& callback) = 0;

  // Indicates that sync is being disabled permanently for this data type. All
  // metadata should be erased from storage.
  virtual void DisableSync() = 0;

  // Returns a boolean representing whether the processor's metadata is
  // currently up to date and accurately tracking the model type's data. If
  // false, and ModelReadyToSync() has already been called, then Put and Delete
  // will no-op and can be omitted by bridge.
  virtual bool IsTrackingMetadata() = 0;

  // Report an error in the model to sync. Should be called for any persistence
  // or consistency error the bridge encounters outside of a method that allows
  // returning a ModelError directly. Outstanding callbacks are not expected to
  // be called after an error. This will result in sync being temporarily
  // disabled for the model type (generally until the next restart).
  virtual void ReportError(const ModelError& error) = 0;

  // A convenience form of the above.
  virtual void ReportError(const tracked_objects::Location& location,
                           const std::string& message) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_CHANGE_PROCESSOR_H_
