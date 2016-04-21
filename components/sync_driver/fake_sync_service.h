// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FAKE_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_FAKE_SYNC_SERVICE_H_

#include <string>

#include "components/sync_driver/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"

namespace syncer {
class BaseTransaction;
struct UserShare;
}

namespace sync_driver {

// Fake implementation of sync_driver::SyncService, used for testing.
class FakeSyncService : public sync_driver::SyncService {
 public:
  FakeSyncService();
  ~FakeSyncService() override;

 private:
  // sync_driver::SyncService:
  bool IsFirstSetupComplete() const override;
  bool IsSyncAllowed() const override;
  bool IsSyncActive() const override;
  void TriggerRefresh(const syncer::ModelTypeSet& types) override;
  syncer::ModelTypeSet GetActiveDataTypes() const override;
  SyncClient* GetSyncClient() const override;
  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;
  void OnDataTypeRequestsSyncStartup(syncer::ModelType type) override;
  bool CanSyncStart() const override;
  void RequestStop(
      sync_driver::SyncService::SyncStopDataFate data_fate) override;
  void RequestStart() override;
  syncer::ModelTypeSet GetPreferredDataTypes() const override;
  void OnUserChoseDatatypes(bool sync_everything,
                            syncer::ModelTypeSet chosen_types) override;
  void SetFirstSetupComplete() override;
  bool IsFirstSetupInProgress() const override;
  void SetSetupInProgress(bool setup_in_progress) override;
  bool IsSetupInProgress() const override;
  bool ConfigurationDone() const override;
  const GoogleServiceAuthError& GetAuthError() const override;
  bool HasUnrecoverableError() const override;
  bool IsBackendInitialized() const override;
  OpenTabsUIDelegate* GetOpenTabsUIDelegate() override;
  bool IsPassphraseRequiredForDecryption() const override;
  base::Time GetExplicitPassphraseTime() const override;
  bool IsUsingSecondaryPassphrase() const override;
  void EnableEncryptEverything() override;
  bool IsEncryptEverythingEnabled() const override;
  void SetEncryptionPassphrase(const std::string& passphrase,
                               PassphraseType type) override;
  bool SetDecryptionPassphrase(const std::string& passphrase) override;
  bool IsCryptographerReady(
      const syncer::BaseTransaction* trans) const override;
  syncer::UserShare* GetUserShare() const override;
  LocalDeviceInfoProvider* GetLocalDeviceInfoProvider() const override;
  void RegisterDataTypeController(
      sync_driver::DataTypeController* data_type_controller) override;
  void ReenableDatatype(syncer::ModelType type) override;
  SyncTokenStatus GetSyncTokenStatus() const override;
  std::string QuerySyncStatusSummaryString() override;
  bool QueryDetailedSyncStatus(syncer::SyncStatus* result) override;
  base::string16 GetLastSyncedTimeString() const override;
  std::string GetBackendInitializationStateString() const override;
  syncer::sessions::SyncSessionSnapshot GetLastSessionSnapshot() const override;
  base::Value* GetTypeStatusMap() const override;
  const GURL& sync_service_url() const override;
  std::string unrecoverable_error_message() const override;
  tracked_objects::Location unrecoverable_error_location() const override;
  void AddProtocolEventObserver(
      browser_sync::ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(
      browser_sync::ProtocolEventObserver* observer) override;
  void AddTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  void RemoveTypeDebugInfoObserver(
      syncer::TypeDebugInfoObserver* observer) override;
  base::WeakPtr<syncer::JsController> GetJsController() override;
  void GetAllNodes(const base::Callback<void(std::unique_ptr<base::ListValue>)>&
                       callback) override;

  // DataTypeEncryptionHandler:
  bool IsPassphraseRequired() const override;
  syncer::ModelTypeSet GetEncryptedDataTypes() const override;

  GoogleServiceAuthError error_;
  GURL sync_service_url_;
  std::string unrecoverable_error_message_;
  std::unique_ptr<syncer::UserShare> user_share_;
};

}  // namespace sync_driver

#endif  // COMPONENTS_SYNC_DRIVER_FAKE_SYNC_SERVICE_H_
