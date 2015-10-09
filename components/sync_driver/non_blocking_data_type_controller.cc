// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/non_blocking_data_type_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/shared_model_type_processor.h"

namespace sync_driver_v2 {

NonBlockingDataTypeController::NonBlockingDataTypeController(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    syncer::ModelType type,
    bool is_preferred)
    : base::RefCountedDeleteOnMessageLoop<NonBlockingDataTypeController>(
          ui_thread),
      type_(type),
      current_state_(DISCONNECTED),
      is_preferred_(is_preferred) {}

NonBlockingDataTypeController::~NonBlockingDataTypeController() {}

void NonBlockingDataTypeController::InitializeType(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::WeakPtr<syncer_v2::SharedModelTypeProcessor>& type_processor) {
  DCHECK(!IsSyncProxyConnected());
  model_task_runner_ = task_runner;
  type_processor_ = type_processor;
  DCHECK(IsSyncProxyConnected());

  UpdateState();
}

void NonBlockingDataTypeController::InitializeSyncContext(
    scoped_ptr<syncer_v2::SyncContextProxy> sync_context_proxy) {
  DCHECK(!IsSyncBackendConnected());
  sync_context_proxy_ = sync_context_proxy.Pass();

  UpdateState();
}

void NonBlockingDataTypeController::ClearSyncContext() {
  // Never had a sync context proxy to begin with.  No change.
  if (!sync_context_proxy_)
    return;
  sync_context_proxy_.reset();

  UpdateState();
}

void NonBlockingDataTypeController::SetIsPreferred(bool is_preferred) {
  is_preferred_ = is_preferred;

  UpdateState();
}

void NonBlockingDataTypeController::UpdateState() {
  // Return immediately if no updates are necessary.
  if (GetDesiredState() == current_state_) {
    return;
  }

  // Return immediately if the sync context proxy is not ready yet.
  if (!IsSyncProxyConnected()) {
    return;
  }

  // Send the appropriate state transition request to the type sync proxy.
  switch (GetDesiredState()) {
    case ENABLED:
      SendEnableSignal();
      return;
    case DISABLED:
      SendDisableSignal();
      return;
    case DISCONNECTED:
      SendDisconnectSignal();
      return;
  }
  NOTREACHED();
  return;
}

void NonBlockingDataTypeController::SendEnableSignal() {
  DCHECK_EQ(ENABLED, GetDesiredState());
  DVLOG(1) << "Enabling non-blocking sync type " << ModelTypeToString(type_);

  model_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingDataTypeController::StartProcessor, this));
  current_state_ = ENABLED;
}

void NonBlockingDataTypeController::SendDisableSignal() {
  DCHECK_EQ(DISABLED, GetDesiredState());
  DVLOG(1) << "Disabling non-blocking sync type " << ModelTypeToString(type_);
  model_task_runner_->PostTask(
      FROM_HERE, base::Bind(&syncer_v2::SharedModelTypeProcessor::Disable,
                            type_processor_));
  current_state_ = DISABLED;
}

void NonBlockingDataTypeController::SendDisconnectSignal() {
  DCHECK_EQ(DISCONNECTED, GetDesiredState());
  DVLOG(1) << "Disconnecting non-blocking sync type "
           << ModelTypeToString(type_);
  model_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingDataTypeController::StopProcessor, this));
  current_state_ = DISCONNECTED;
}

void NonBlockingDataTypeController::StartProcessor() {
  DCHECK(model_task_runner_->BelongsToCurrentThread());
  type_processor_->Start(
      base::Bind(&NonBlockingDataTypeController::OnProcessorStarted, this));
}

void NonBlockingDataTypeController::OnProcessorStarted(
    /*syncer::SyncError error,*/
    scoped_ptr<syncer_v2::ActivationContext> activation_context) {
  DCHECK(model_task_runner_->BelongsToCurrentThread());
  sync_context_proxy_->ConnectTypeToSync(type_, activation_context.Pass());
}

void NonBlockingDataTypeController::StopProcessor() {
  DCHECK(model_task_runner_->BelongsToCurrentThread());
  type_processor_->Stop();
}

bool NonBlockingDataTypeController::IsPreferred() const {
  return is_preferred_;
}

bool NonBlockingDataTypeController::IsSyncProxyConnected() const {
  return model_task_runner_.get() != NULL;
}

bool NonBlockingDataTypeController::IsSyncBackendConnected() const {
  return sync_context_proxy_;
}

NonBlockingDataTypeController::TypeState
NonBlockingDataTypeController::GetDesiredState() const {
  if (!IsPreferred()) {
    return DISABLED;
  } else if (!IsSyncBackendConnected() || !IsSyncProxyConnected()) {
    return DISCONNECTED;
  } else {
    return ENABLED;
  }
}

}  // namespace sync_driver_v2
