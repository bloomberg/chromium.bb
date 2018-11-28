// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/test_sync_service.h"

#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/base/progress_marker_map.h"
#include "components/sync/driver/sync_token_status.h"
#include "components/sync/engine/cycle/model_neutral_state.h"

namespace syncer {

namespace {

SyncCycleSnapshot MakeDefaultCycleSnapshot() {
  return SyncCycleSnapshot(
      ModelNeutralState(), ProgressMarkerMap(), /*is_silenced-*/ false,
      /*num_encryption_conflicts=*/5, /*num_hierarchy_conflicts=*/2,
      /*num_server_conflicts=*/7, /*notifications_enabled=*/false,
      /*num_entries=*/0, /*sync_start_time=*/base::Time::Now(),
      /*poll_finish_time=*/base::Time::Now(),
      /*num_entries_by_type=*/std::vector<int>(MODEL_TYPE_COUNT, 0),
      /*num_to_delete_entries_by_type=*/
      std::vector<int>(MODEL_TYPE_COUNT, 0),
      /*get_updates_origin=*/sync_pb::SyncEnums::UNKNOWN_ORIGIN,
      /*short_poll_interval=*/base::TimeDelta::FromMinutes(30),
      /*long_poll_interval=*/base::TimeDelta::FromMinutes(180),
      /*has_remaining_local_changes=*/false);
}

}  // namespace

TestSyncService::TestSyncService()
    : user_settings_(this),
      preferred_data_types_(ModelTypeSet::All()),
      active_data_types_(ModelTypeSet::All()),
      last_cycle_snapshot_(MakeDefaultCycleSnapshot()) {}

TestSyncService::~TestSyncService() = default;

void TestSyncService::SetDisableReasons(int disable_reasons) {
  disable_reasons_ = disable_reasons;
}

void TestSyncService::SetTransportState(TransportState transport_state) {
  transport_state_ = transport_state;
}

void TestSyncService::SetLocalSyncEnabled(bool local_sync_enabled) {
  local_sync_enabled_ = local_sync_enabled;
}

void TestSyncService::SetAuthenticatedAccountInfo(
    const AccountInfo& account_info) {
  account_info_ = account_info;
}

void TestSyncService::SetIsAuthenticatedAccountPrimary(bool is_primary) {
  account_is_primary_ = is_primary;
}

void TestSyncService::SetAuthError(const GoogleServiceAuthError& auth_error) {
  auth_error_ = auth_error;
}

void TestSyncService::SetFirstSetupComplete(bool first_setup_complete) {
  first_setup_complete_ = first_setup_complete;
}

void TestSyncService::SetPreferredDataTypes(const ModelTypeSet& types) {
  preferred_data_types_ = types;
}

void TestSyncService::SetActiveDataTypes(const ModelTypeSet& types) {
  active_data_types_ = types;
}

void TestSyncService::SetIsUsingSecondaryPassphrase(bool enabled) {
  using_secondary_passphrase_ = enabled;
}

void TestSyncService::SetLastCycleSnapshot(const SyncCycleSnapshot& snapshot) {
  last_cycle_snapshot_ = snapshot;
}

void TestSyncService::SetEmptyLastCycleSnapshot() {
  SetLastCycleSnapshot(SyncCycleSnapshot());
}

void TestSyncService::SetNonEmptyLastCycleSnapshot() {
  SetLastCycleSnapshot(MakeDefaultCycleSnapshot());
}

SyncUserSettings* TestSyncService::GetUserSettings() {
  return &user_settings_;
}

const SyncUserSettings* TestSyncService::GetUserSettings() const {
  return &user_settings_;
}

int TestSyncService::GetDisableReasons() const {
  return disable_reasons_;
}

SyncService::TransportState TestSyncService::GetTransportState() const {
  return transport_state_;
}

bool TestSyncService::IsLocalSyncEnabled() const {
  return local_sync_enabled_;
}

AccountInfo TestSyncService::GetAuthenticatedAccountInfo() const {
  return account_info_;
}

bool TestSyncService::IsAuthenticatedAccountPrimary() const {
  return account_is_primary_;
}

const GoogleServiceAuthError& TestSyncService::GetAuthError() const {
  return auth_error_;
}

bool TestSyncService::IsFirstSetupComplete() const {
  return first_setup_complete_;
}

std::unique_ptr<SyncSetupInProgressHandle>
TestSyncService::GetSetupInProgressHandle() {
  return nullptr;
}

bool TestSyncService::IsSetupInProgress() const {
  return false;
}

ModelTypeSet TestSyncService::GetPreferredDataTypes() const {
  return preferred_data_types_;
}

ModelTypeSet TestSyncService::GetActiveDataTypes() const {
  return active_data_types_;
}

void TestSyncService::RequestStop(SyncService::SyncStopDataFate data_fate) {}

void TestSyncService::OnDataTypeRequestsSyncStartup(ModelType type) {}

void TestSyncService::TriggerRefresh(const ModelTypeSet& types) {}

void TestSyncService::ReenableDatatype(ModelType type) {}

void TestSyncService::ReadyForStartChanged(ModelType type) {}

void TestSyncService::AddObserver(SyncServiceObserver* observer) {}

void TestSyncService::RemoveObserver(SyncServiceObserver* observer) {}

bool TestSyncService::HasObserver(const SyncServiceObserver* observer) const {
  return false;
}

bool TestSyncService::IsPassphraseRequiredForDecryption() const {
  return false;
}

base::Time TestSyncService::GetExplicitPassphraseTime() const {
  return base::Time();
}

bool TestSyncService::IsUsingSecondaryPassphrase() const {
  return using_secondary_passphrase_;
}

void TestSyncService::EnableEncryptEverything() {}

bool TestSyncService::IsEncryptEverythingEnabled() const {
  return false;
}

void TestSyncService::SetEncryptionPassphrase(const std::string& passphrase) {}

bool TestSyncService::SetDecryptionPassphrase(const std::string& passphrase) {
  return false;
}

UserShare* TestSyncService::GetUserShare() const {
  return nullptr;
}

SyncTokenStatus TestSyncService::GetSyncTokenStatus() const {
  SyncTokenStatus token;

  if (GetAuthError().state() != GoogleServiceAuthError::NONE) {
    token.connection_status = ConnectionStatus::CONNECTION_AUTH_ERROR;
    token.last_get_token_error =
        GoogleServiceAuthError::FromServiceError("error");
  }

  return token;
}

bool TestSyncService::QueryDetailedSyncStatus(SyncStatus* result) const {
  return false;
}

base::Time TestSyncService::GetLastSyncedTime() const {
  return base::Time();
}

SyncCycleSnapshot TestSyncService::GetLastCycleSnapshot() const {
  return last_cycle_snapshot_;
}

std::unique_ptr<base::Value> TestSyncService::GetTypeStatusMap() {
  return std::make_unique<base::ListValue>();
}

const GURL& TestSyncService::sync_service_url() const {
  return sync_service_url_;
}

std::string TestSyncService::unrecoverable_error_message() const {
  return std::string();
}

base::Location TestSyncService::unrecoverable_error_location() const {
  return base::Location();
}

void TestSyncService::AddProtocolEventObserver(
    ProtocolEventObserver* observer) {}

void TestSyncService::RemoveProtocolEventObserver(
    ProtocolEventObserver* observer) {}

void TestSyncService::AddTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {}

void TestSyncService::RemoveTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {}

base::WeakPtr<JsController> TestSyncService::GetJsController() {
  return base::WeakPtr<JsController>();
}

void TestSyncService::GetAllNodes(
    const base::Callback<void(std::unique_ptr<base::ListValue>)>& callback) {}

bool TestSyncService::IsPassphraseRequired() const {
  return false;
}

ModelTypeSet TestSyncService::GetEncryptedDataTypes() const {
  if (!using_secondary_passphrase_) {
    // PASSWORDS are always encrypted.
    return ModelTypeSet(PASSWORDS);
  }
  // Some types can never be encrypted, e.g. DEVICE_INFO and
  // AUTOFILL_WALLET_DATA, so make sure we don't report them as encrypted.
  return Intersection(GetPreferredDataTypes(), EncryptableUserTypes());
}

void TestSyncService::Shutdown() {}

}  // namespace syncer
