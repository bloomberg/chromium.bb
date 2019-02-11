// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/sync_service_observer.h"

namespace syncer {
class ModelTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace password_manager {

// A class that manages the startup and shutdown of password sync.
class PasswordModelTypeController : public syncer::ModelTypeController,
                                    public syncer::SyncServiceObserver {
 public:
  PasswordModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate_on_disk,
      syncer::SyncService* sync_service,
      const base::RepeatingClosure& state_changed_callback);
  ~PasswordModelTypeController() override;

  // DataTypeController overrides.
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void Stop(syncer::ShutdownReason shutdown_reason,
            StopCallback callback) override;

  // SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  syncer::SyncService* const sync_service_;
  const base::RepeatingClosure state_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(PasswordModelTypeController);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_MODEL_TYPE_CONTROLLER_H_
