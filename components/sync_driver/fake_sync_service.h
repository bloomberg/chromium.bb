// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FAKE_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_FAKE_SYNC_SERVICE_H_

#include "components/sync_driver/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace sync_driver {

// Fake implementation of sync_driver::SyncService, used for testing.
class FakeSyncService : public sync_driver::SyncService {
 public:
  FakeSyncService();
  ~FakeSyncService() override;

 private:
  // sync_driver::SyncService:
  bool HasSyncSetupCompleted() const override;
  bool IsSyncAllowed() const override;
  bool IsSyncActive() const override;
  syncer::ModelTypeSet GetActiveDataTypes() const override;
  void AddObserver(sync_driver::SyncServiceObserver* observer) override;
  void RemoveObserver(sync_driver::SyncServiceObserver* observer) override;
  bool HasObserver(
      const sync_driver::SyncServiceObserver* observer) const override;
  bool CanSyncStart() const override;
  void RequestStop(
      sync_driver::SyncService::SyncStopDataFate data_fate) override;
  void RequestStart() override;
  syncer::ModelTypeSet GetPreferredDataTypes() const override;
  void OnUserChoseDatatypes(bool sync_everything,
                            syncer::ModelTypeSet chosen_types) override;
  void SetSyncSetupCompleted() override;
  bool FirstSetupInProgress() const override;
  void SetSetupInProgress(bool setup_in_progress) override;
  bool setup_in_progress() const override;
  bool ConfigurationDone() const override;
  const GoogleServiceAuthError& GetAuthError() const override;
  bool HasUnrecoverableError() const override;
  bool backend_initialized() const override;
  bool IsPassphraseRequiredForDecryption() const override;
  base::Time GetExplicitPassphraseTime() const override;
  bool IsUsingSecondaryPassphrase() const override;
  void EnableEncryptEverything() override;
  void SetEncryptionPassphrase(const std::string& passphrase,
                               PassphraseType type) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;

  // sync_driver::DataTypeEncryptionHandler:
  bool IsPassphraseRequired() const override;
  syncer::ModelTypeSet GetEncryptedDataTypes() const override;

  GoogleServiceAuthError error_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_FAKE_SYNC_SERVICE_H_
