// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_model_type_controller.h"

#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/model_type_controller_delegate.h"

namespace password_manager {

PasswordModelTypeController::PasswordModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_on_disk,
    syncer::SyncService* sync_service,
    syncer::SyncClient* sync_client)
    : ModelTypeController(syncer::PASSWORDS, std::move(delegate_on_disk)),
      sync_service_(sync_service),
      sync_client_(sync_client) {}

PasswordModelTypeController::~PasswordModelTypeController() = default;

void PasswordModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  sync_service_->AddObserver(this);
  ModelTypeController::LoadModels(configure_context, model_load_callback);
  sync_client_->GetPasswordStateChangedCallback().Run();
}

void PasswordModelTypeController::Stop(syncer::ShutdownReason shutdown_reason,
                                       StopCallback callback) {
  DCHECK(CalledOnValidThread());
  sync_service_->RemoveObserver(this);
  ModelTypeController::Stop(shutdown_reason, std::move(callback));
  sync_client_->GetPasswordStateChangedCallback().Run();
}

void PasswordModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_client_->GetPasswordStateChangedCallback().Run();
}

}  // namespace password_manager
