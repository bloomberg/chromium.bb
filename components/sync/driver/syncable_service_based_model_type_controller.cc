// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/syncable_service_based_model_type_controller.h"

#include <memory>
#include <utility>

#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/forwarding_model_type_controller_delegate.h"
#include "components/sync/model_impl/syncable_service_based_bridge.h"

namespace syncer {

namespace {

// Similar to ForwardingModelTypeControllerDelegate, but allows evaluating the
// reference to SyncableService in a lazy way, which is convenient for tests.
class ControllerDelegate : public ModelTypeControllerDelegate {
 public:
  ControllerDelegate(ModelType type,
                     OnceModelTypeStoreFactory store_factory,
                     base::WeakPtr<SyncableService> syncable_service,
                     const base::RepeatingClosure& dump_stack)
      : type_(type),
        dump_stack_(dump_stack) {
    DCHECK(store_factory);

    // The |syncable_service| can be null in tests.
    if (syncable_service) {
      bridge_ = std::make_unique<SyncableServiceBasedBridge>(
          type_, std::move(store_factory),
          std::make_unique<ClientTagBasedModelTypeProcessor>(type_,
                                                             dump_stack_),
          syncable_service.get());
    }
  }

  ~ControllerDelegate() override {}

  void OnSyncStarting(const DataTypeActivationRequest& request,
                      StartCallback callback) override {
    GetBridgeDelegate()->OnSyncStarting(request, std::move(callback));
  }

  void OnSyncStopping(SyncStopMetadataFate metadata_fate) override {
    GetBridgeDelegate()->OnSyncStopping(metadata_fate);
  }

  void GetAllNodesForDebugging(AllNodesCallback callback) override {
    GetBridgeDelegate()->GetAllNodesForDebugging(std::move(callback));
  }

  void GetStatusCountersForDebugging(StatusCountersCallback callback) override {
    GetBridgeDelegate()->GetStatusCountersForDebugging(std::move(callback));
  }

  void RecordMemoryUsageAndCountsHistograms() override {
    GetBridgeDelegate()->RecordMemoryUsageAndCountsHistograms();
  }

 private:
  ModelTypeControllerDelegate* GetBridgeDelegate() {
    DCHECK(bridge_);
    return bridge_->change_processor()->GetControllerDelegate().get();
  }

  const ModelType type_;
  const base::RepeatingClosure dump_stack_;
  std::unique_ptr<ModelTypeSyncBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(ControllerDelegate);
};

}  // namespace

SyncableServiceBasedModelTypeController::
    SyncableServiceBasedModelTypeController(
        ModelType type,
        OnceModelTypeStoreFactory store_factory,
        base::WeakPtr<SyncableService> syncable_service,
        const base::RepeatingClosure& dump_stack,
        DelegateMode delegate_mode)
    : ModelTypeController(type),
      delegate_(std::make_unique<ControllerDelegate>(type,
                                                     std::move(store_factory),
                                                     syncable_service,
                                                     dump_stack)) {
  // Delegate for full sync is always created.
  auto full_sync_delegate =
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
          delegate_.get());

  // Delegate for transport is only created if requested and left null
  // otherwise.
  std::unique_ptr<syncer::ForwardingModelTypeControllerDelegate>
      transport_delegate;
  if (delegate_mode == DelegateMode::kTransportModeWithSingleModel) {
    transport_delegate =
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate_.get());
  }

  InitModelTypeController(std::move(full_sync_delegate),
                          std::move(transport_delegate));
}

SyncableServiceBasedModelTypeController::
    ~SyncableServiceBasedModelTypeController() = default;

}  // namespace syncer
