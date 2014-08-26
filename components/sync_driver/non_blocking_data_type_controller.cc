// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/non_blocking_data_type_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "sync/engine/model_type_sync_proxy_impl.h"

namespace sync_driver {

NonBlockingDataTypeController::NonBlockingDataTypeController(
    syncer::ModelType type, bool is_preferred)
    : type_(type),
      current_state_(DISCONNECTED),
      is_preferred_(is_preferred) {}

NonBlockingDataTypeController::~NonBlockingDataTypeController() {}

void NonBlockingDataTypeController::InitializeType(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::WeakPtr<syncer::ModelTypeSyncProxyImpl>& type_sync_proxy) {
  DCHECK(!IsSyncProxyConnected());
  task_runner_ = task_runner;
  type_sync_proxy_ = type_sync_proxy;
  DCHECK(IsSyncProxyConnected());

  UpdateState();
}

void NonBlockingDataTypeController::InitializeSyncContext(
    scoped_ptr<syncer::SyncContextProxy> sync_context_proxy) {
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

  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&syncer::ModelTypeSyncProxyImpl::Enable,
                 type_sync_proxy_,
                 base::Passed(sync_context_proxy_->Clone())));
  current_state_ = ENABLED;
}

void NonBlockingDataTypeController::SendDisableSignal() {
  DCHECK_EQ(DISABLED, GetDesiredState());
  DVLOG(1) << "Disabling non-blocking sync type " << ModelTypeToString(type_);
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&syncer::ModelTypeSyncProxyImpl::Disable, type_sync_proxy_));
  current_state_ = DISABLED;
}

void NonBlockingDataTypeController::SendDisconnectSignal() {
  DCHECK_EQ(DISCONNECTED, GetDesiredState());
  DVLOG(1) << "Disconnecting non-blocking sync type "
           << ModelTypeToString(type_);
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&syncer::ModelTypeSyncProxyImpl::Disconnect,
                                    type_sync_proxy_));
  current_state_ = DISCONNECTED;
}

bool NonBlockingDataTypeController::IsPreferred() const {
  return is_preferred_;
}

bool NonBlockingDataTypeController::IsSyncProxyConnected() const {
  return task_runner_.get() != NULL;
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

}  // namespace sync_driver
