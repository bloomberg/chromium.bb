// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/directory_data_type_controller.h"

#include <utility>

#include "components/sync/core/user_share.h"
#include "components/sync/driver/backend_data_type_configurer.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/syncable/syncable_read_transaction.h"

namespace syncer {

DirectoryDataTypeController::DirectoryDataTypeController(
    ModelType type,
    const base::Closure& dump_stack,
    SyncClient* sync_client)
    : DataTypeController(type, dump_stack), sync_client_(sync_client) {}

DirectoryDataTypeController::~DirectoryDataTypeController() {}

bool DirectoryDataTypeController::ShouldLoadModelBeforeConfigure() const {
  // Directory datatypes don't require loading models before configure. Their
  // progress markers are stored in directory and can be extracted without
  // datatype participation.
  return false;
}

void DirectoryDataTypeController::GetAllNodes(
    const AllNodesCallback& callback) {
  std::unique_ptr<base::ListValue> node_list = GetAllNodesForTypeFromDirectory(
      type(), sync_client_->GetSyncService()->GetUserShare()->directory.get());
  callback.Run(type(), std::move(node_list));
}

void DirectoryDataTypeController::RegisterWithBackend(
    BackendDataTypeConfigurer* configurer) {}

void DirectoryDataTypeController::ActivateDataType(
    BackendDataTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  // Tell the backend about the change processor for this type so it can
  // begin routing changes to it.
  configurer->ActivateDirectoryDataType(type(), model_safe_group(),
                                        GetChangeProcessor());
}

void DirectoryDataTypeController::DeactivateDataType(
    BackendDataTypeConfigurer* configurer) {
  DCHECK(CalledOnValidThread());
  configurer->DeactivateDirectoryDataType(type());
}

// static
std::unique_ptr<base::ListValue>
DirectoryDataTypeController::GetAllNodesForTypeFromDirectory(
    ModelType type,
    syncable::Directory* directory) {
  DCHECK(directory);
  syncable::ReadTransaction trans(FROM_HERE, directory);
  return directory->GetNodeDetailsForType(&trans, type);
}

}  // namespace syncer
