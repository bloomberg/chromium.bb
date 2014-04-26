// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/non_blocking_data_type_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "sync/internal_api/public/non_blocking_type_processor.h"

namespace browser_sync {

NonBlockingDataTypeController::NonBlockingDataTypeController(
    syncer::ModelType type, bool is_preferred)
    : type_(type),
      current_state_(DISCONNECTED),
      is_preferred_(is_preferred) {}

NonBlockingDataTypeController::~NonBlockingDataTypeController() {}

void NonBlockingDataTypeController::InitializeProcessor(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::WeakPtr<syncer::NonBlockingTypeProcessor> processor) {
  DCHECK(!IsTypeProcessorConnected());
  task_runner_ = task_runner;
  processor_ = processor;
  DCHECK(IsTypeProcessorConnected());

  UpdateState();
}

void NonBlockingDataTypeController::InitializeSyncCoreProxy(
    scoped_ptr<syncer::SyncCoreProxy> proxy) {
  DCHECK(!IsSyncBackendConnected());
  proxy_ = proxy.Pass();

  UpdateState();
}

void NonBlockingDataTypeController::ClearSyncCoreProxy() {
  // Never had a sync core proxy to begin with.  No change.
  if (!proxy_)
    return;
  proxy_.reset();

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

  // Return immediately if the processor is not ready yet.
  if (!IsTypeProcessorConnected()) {
    return;
  }

  // Send the appropriate state transition request to the processor.
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

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&syncer::NonBlockingTypeProcessor::Enable,
                 processor_,
                 base::Owned(proxy_->Clone().release())));
  current_state_ = ENABLED;
}

void NonBlockingDataTypeController::SendDisableSignal() {
  DCHECK_EQ(DISABLED, GetDesiredState());
  DVLOG(1) << "Disabling non-blocking sync type " << ModelTypeToString(type_);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&syncer::NonBlockingTypeProcessor::Disable, processor_));
  current_state_ = DISABLED;
}

void NonBlockingDataTypeController::SendDisconnectSignal() {
  DCHECK_EQ(DISCONNECTED, GetDesiredState());
  DVLOG(1) << "Disconnecting non-blocking sync type "
           << ModelTypeToString(type_);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&syncer::NonBlockingTypeProcessor::Disconnect, processor_));
  current_state_ = DISCONNECTED;
}

bool NonBlockingDataTypeController::IsPreferred() const {
  return is_preferred_;
}

bool NonBlockingDataTypeController::IsTypeProcessorConnected() const {
  return task_runner_ != NULL;
}

bool NonBlockingDataTypeController::IsSyncBackendConnected() const {
  return proxy_;
}

NonBlockingDataTypeController::TypeProcessorState
NonBlockingDataTypeController::GetDesiredState() const {
  if (!IsPreferred()) {
    return DISABLED;
  } else if (!IsSyncBackendConnected() || !IsTypeProcessorConnected()) {
    return DISCONNECTED;
  } else {
    return ENABLED;
  }
}

}  // namespace browser_sync
