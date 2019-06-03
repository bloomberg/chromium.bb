// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/non_ui_syncable_service_based_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/proxy_model_type_controller_delegate.h"
#include "components/sync/model_impl/syncable_service_based_bridge.h"

namespace syncer {

namespace {

// Helper object that allows constructing and destructing the
// SyncableServiceBasedBridge on the model thread.
class BridgeBuilder {
 public:
  BridgeBuilder(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      NonUiSyncableServiceBasedModelTypeController::SyncableServiceProvider
          syncable_service_provider,
      const base::RepeatingClosure& dump_stack,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : task_runner_(task_runner),
        bridge_(nullptr, base::OnTaskRunnerDeleter(task_runner)),
        weak_ptr_factory_(this) {
    DCHECK(store_factory);
    DCHECK(syncable_service_provider);

    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&BridgeBuilder::BuildBridgeDelegate,
                       weak_ptr_factory_.GetWeakPtr(), type,
                       std::move(store_factory),
                       std::move(syncable_service_provider), dump_stack));
  }

  base::WeakPtr<ModelTypeControllerDelegate> GetBridgeDelegate() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(bridge_);
    return bridge_->change_processor()->GetControllerDelegate();
  }

 private:
  void BuildBridgeDelegate(
      ModelType type,
      OnceModelTypeStoreFactory store_factory,
      NonUiSyncableServiceBasedModelTypeController::SyncableServiceProvider
          syncable_service_provider,
      const base::RepeatingClosure& dump_stack) {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!bridge_);

    base::WeakPtr<SyncableService> syncable_service =
        std::move(syncable_service_provider).Run();
    // |syncable_service| can be null in tests.
    if (syncable_service) {
      // std::make_unique() avoided here due to custom deleter.
      bridge_.reset(new SyncableServiceBasedBridge(
          type, std::move(store_factory),
          std::make_unique<ClientTagBasedModelTypeProcessor>(type, dump_stack),
          syncable_service.get()));
    }
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<ModelTypeSyncBridge, base::OnTaskRunnerDeleter> bridge_;

  base::WeakPtrFactory<BridgeBuilder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BridgeBuilder);
};

}  // namespace

NonUiSyncableServiceBasedModelTypeController::
    NonUiSyncableServiceBasedModelTypeController(
        ModelType type,
        OnceModelTypeStoreFactory store_factory,
        SyncableServiceProvider syncable_service_provider,
        const base::RepeatingClosure& dump_stack,
        scoped_refptr<base::SequencedTaskRunner> task_runner)
    : ModelTypeController(
          type,
          std::make_unique<ProxyModelTypeControllerDelegate>(
              task_runner,
              base::BindRepeating(&BridgeBuilder::GetBridgeDelegate,
                                  std::make_unique<BridgeBuilder>(
                                      type,
                                      std::move(store_factory),
                                      std::move(syncable_service_provider),
                                      dump_stack,
                                      task_runner)))) {}

NonUiSyncableServiceBasedModelTypeController::
    ~NonUiSyncableServiceBasedModelTypeController() {}

}  // namespace syncer
