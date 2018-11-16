// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FAKE_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_FAKE_SYNC_SERVICE_H_

#include <memory>
#include <string>

#include "components/sync/driver/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace syncer {

// Minimal fake implementation of SyncService. All methods return inactive/
// empty/null etc. Tests can subclass this to override the parts they need, but
// should consider using TestSyncService instead.
class FakeSyncService : public SyncService {
 public:
  FakeSyncService();
  ~FakeSyncService() override;

  // Dummy methods.
  // SyncService implementation.
  syncer::SyncUserSettings* GetUserSettings() override;
  const syncer::SyncUserSettings* GetUserSettings() const override;
  int GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  AccountInfo GetAuthenticatedAccountInfo() const override;
  bool IsAuthenticatedAccountPrimary() const override;
  bool IsFirstSetupComplete() const override;
  bool IsLocalSyncEnabled() const override;
  void TriggerRefresh(const ModelTypeSet& types) override;
  ModelTypeSet GetActiveDataTypes() const override;
  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;
  void OnDataTypeRequestsSyncStartup(ModelType type) override;
  void RequestStop(SyncService::SyncStopDataFate data_fate) override;
  void RequestStart() override;
  ModelTypeSet GetPreferredDataTypes() const override;
  void OnUserChoseDatatypes(bool sync_everything,
                            ModelTypeSet chosen_types) override;
  void SetFirstSetupComplete() override;
  std::unique_ptr<SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  bool IsSetupInProgress() const override;
  const GoogleServiceAuthError& GetAuthError() const override;
  bool IsPassphraseRequiredForDecryption() const override;
  base::Time GetExplicitPassphraseTime() const override;
  bool IsUsingSecondaryPassphrase() const override;
  void EnableEncryptEverything() override;
  bool IsEncryptEverythingEnabled() const override;
  void SetEncryptionPassphrase(const std::string& passphrase) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;
  UserShare* GetUserShare() const override;
  void ReenableDatatype(ModelType type) override;
  void ReadyForStartChanged(syncer::ModelType type) override;
  SyncTokenStatus GetSyncTokenStatus() const override;
  bool QueryDetailedSyncStatus(SyncStatus* result) const override;
  base::Time GetLastSyncedTime() const override;
  SyncCycleSnapshot GetLastCycleSnapshot() const override;
  std::unique_ptr<base::Value> GetTypeStatusMap() override;
  const GURL& sync_service_url() const override;
  std::string unrecoverable_error_message() const override;
  base::Location unrecoverable_error_location() const override;
  void AddProtocolEventObserver(ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(ProtocolEventObserver* observer) override;
  void AddTypeDebugInfoObserver(TypeDebugInfoObserver* observer) override;
  void RemoveTypeDebugInfoObserver(TypeDebugInfoObserver* observer) override;
  base::WeakPtr<JsController> GetJsController() override;
  void GetAllNodes(const base::Callback<void(std::unique_ptr<base::ListValue>)>&
                       callback) override;

  // DataTypeEncryptionHandler implementation.
  bool IsPassphraseRequired() const override;
  ModelTypeSet GetEncryptedDataTypes() const override;

  // KeyedService implementation.
  void Shutdown() override;

 private:
  GoogleServiceAuthError error_;
  GURL sync_service_url_;
  std::unique_ptr<UserShare> user_share_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_FAKE_SYNC_SERVICE_H_
