// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_TEST_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_DRIVER_TEST_SYNC_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/test_sync_user_settings.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/sync_status.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "url/gurl.h"

namespace syncer {

// A simple test implementation of SyncService that allows direct control over
// the returned state. By default, everything returns "enabled"/"active".
class TestSyncService : public SyncService {
 public:
  TestSyncService();
  ~TestSyncService() override;

  void SetDisableReasons(DisableReasonSet disable_reasons);
  void SetTransportState(TransportState transport_state);
  void SetLocalSyncEnabled(bool local_sync_enabled);
  void SetAuthenticatedAccountInfo(const CoreAccountInfo& account_info);
  void SetIsAuthenticatedAccountPrimary(bool is_primary);
  void SetSetupInProgress(bool in_progress);
  void SetAuthError(const GoogleServiceAuthError& auth_error);
  void SetFirstSetupComplete(bool first_setup_complete);
  void SetPreferredDataTypes(const ModelTypeSet& types);
  void SetActiveDataTypes(const ModelTypeSet& types);
  void SetBackedOffDataTypes(const ModelTypeSet& types);
  void SetLastCycleSnapshot(const SyncCycleSnapshot& snapshot);
  void SetUserDemographics(
      const UserDemographicsResult& user_demographics_result);
  // Convenience versions of the above, for when the caller doesn't care about
  // the particular values in the snapshot, just whether there is one.
  void SetEmptyLastCycleSnapshot();
  void SetNonEmptyLastCycleSnapshot();
  void SetDetailedSyncStatus(bool engine_available, SyncStatus status);
  void SetPassphraseRequired(bool required);
  void SetPassphraseRequiredForPreferredDataTypes(bool required);
  void SetTrustedVaultKeyRequired(bool required);
  void SetTrustedVaultKeyRequiredForPreferredDataTypes(bool required);
  void SetIsUsingSecondaryPassphrase(bool enabled);

  void FireStateChanged();
  void FireSyncCycleCompleted();

  // SyncService implementation.
  syncer::SyncUserSettings* GetUserSettings() override;
  const syncer::SyncUserSettings* GetUserSettings() const override;
  DisableReasonSet GetDisableReasons() const override;
  TransportState GetTransportState() const override;
  bool IsLocalSyncEnabled() const override;
  CoreAccountInfo GetAuthenticatedAccountInfo() const override;
  bool IsAuthenticatedAccountPrimary() const override;
  GoogleServiceAuthError GetAuthError() const override;
  base::Time GetAuthErrorTime() const override;
  bool RequiresClientUpgrade() const override;

  std::unique_ptr<SyncSetupInProgressHandle> GetSetupInProgressHandle()
      override;
  bool IsSetupInProgress() const override;

  ModelTypeSet GetRegisteredDataTypes() const override;
  ModelTypeSet GetPreferredDataTypes() const override;
  ModelTypeSet GetActiveDataTypes() const override;
  ModelTypeSet GetBackedOffDataTypes() const override;

  void StopAndClear() override;
  void OnDataTypeRequestsSyncStartup(ModelType type) override;
  void TriggerRefresh(const ModelTypeSet& types) override;
  void DataTypePreconditionChanged(syncer::ModelType type) override;

  void AddObserver(SyncServiceObserver* observer) override;
  void RemoveObserver(SyncServiceObserver* observer) override;
  bool HasObserver(const SyncServiceObserver* observer) const override;

  UserShare* GetUserShare() const override;

  SyncTokenStatus GetSyncTokenStatusForDebugging() const override;
  bool QueryDetailedSyncStatusForDebugging(SyncStatus* result) const override;
  base::Time GetLastSyncedTimeForDebugging() const override;
  SyncCycleSnapshot GetLastCycleSnapshotForDebugging() const override;
  std::unique_ptr<base::Value> GetTypeStatusMapForDebugging() override;
  const GURL& GetSyncServiceUrlForDebugging() const override;
  std::string GetUnrecoverableErrorMessageForDebugging() const override;
  base::Location GetUnrecoverableErrorLocationForDebugging() const override;
  void AddProtocolEventObserver(ProtocolEventObserver* observer) override;
  void RemoveProtocolEventObserver(ProtocolEventObserver* observer) override;
  void AddTypeDebugInfoObserver(TypeDebugInfoObserver* observer) override;
  void RemoveTypeDebugInfoObserver(TypeDebugInfoObserver* observer) override;
  base::WeakPtr<JsController> GetJsController() override;
  void GetAllNodesForDebugging(
      base::OnceCallback<void(std::unique_ptr<base::ListValue>)> callback)
      override;
  void SetInvalidationsForSessionsEnabled(bool enabled) override;
  void AddTrustedVaultDecryptionKeysFromWeb(
      const std::string& gaia_id,
      const std::vector<std::vector<uint8_t>>& keys,
      int last_key_version) override;
  UserDemographicsResult GetUserNoisedBirthYearAndGender(
      base::Time now) override;

  // KeyedService implementation.
  void Shutdown() override;

 private:
  TestSyncUserSettings user_settings_;

  DisableReasonSet disable_reasons_;
  TransportState transport_state_ = TransportState::ACTIVE;
  bool local_sync_enabled_ = false;
  CoreAccountInfo account_info_;
  bool account_is_primary_ = true;
  bool setup_in_progress_ = false;
  GoogleServiceAuthError auth_error_;

  ModelTypeSet preferred_data_types_;
  ModelTypeSet active_data_types_;
  ModelTypeSet backed_off_data_types_;

  bool detailed_sync_status_engine_available_ = false;
  SyncStatus detailed_sync_status_;

  SyncCycleSnapshot last_cycle_snapshot_;

  base::ObserverList<syncer::SyncServiceObserver>::Unchecked observers_;

  GURL sync_service_url_;

  UserDemographicsResult user_demographics_result_;

  DISALLOW_COPY_AND_ASSIGN(TestSyncService);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_TEST_SYNC_SERVICE_H_
