// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/test_sync_service.h"

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync/driver/sync_token_status.h"

namespace syncer {

TestSyncService::TestSyncService() = default;

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

void TestSyncService::SetAuthError(const GoogleServiceAuthError& auth_error) {
  auth_error_ = auth_error;
}

void TestSyncService::SetPreferredDataTypes(const ModelTypeSet& types) {
  preferred_data_types_ = types;
}

void TestSyncService::SetActiveDataTypes(const ModelTypeSet& types) {
  active_data_types_ = types;
}

void TestSyncService::SetCustomPassphraseEnabled(bool enabled) {
  custom_passphrase_enabled_ = enabled;
}

void TestSyncService::SetLastCycleSnapshot(const SyncCycleSnapshot& snapshot) {
  last_cycle_snapshot_ = snapshot;
}

int TestSyncService::GetDisableReasons() const {
  return disable_reasons_;
}

syncer::SyncService::TransportState TestSyncService::GetTransportState() const {
  return transport_state_;
}

bool TestSyncService::IsLocalSyncEnabled() const {
  return local_sync_enabled_;
}

AccountInfo TestSyncService::GetAuthenticatedAccountInfo() const {
  return account_info_;
}

bool TestSyncService::IsAuthenticatedAccountPrimary() const {
  return true;
}

const GoogleServiceAuthError& TestSyncService::GetAuthError() const {
  return auth_error_;
}

bool TestSyncService::IsFirstSetupComplete() const {
  return true;
}

void TestSyncService::SetFirstSetupComplete() {}

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

void TestSyncService::RequestStart() {}

void TestSyncService::RequestStop(SyncService::SyncStopDataFate data_fate) {}

void TestSyncService::OnDataTypeRequestsSyncStartup(ModelType type) {}

void TestSyncService::OnUserChoseDatatypes(bool sync_everything,
                                           ModelTypeSet chosen_types) {}

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
  return custom_passphrase_enabled_;
}

void TestSyncService::EnableEncryptEverything() {}

bool TestSyncService::IsEncryptEverythingEnabled() const {
  return false;
}

void TestSyncService::SetEncryptionPassphrase(const std::string& passphrase,
                                              PassphraseType type) {}

bool TestSyncService::SetDecryptionPassphrase(const std::string& passphrase) {
  return false;
}

bool TestSyncService::IsCryptographerReady(const BaseTransaction* trans) const {
  return false;
}

SyncClient* TestSyncService::GetSyncClient() const {
  return nullptr;
}

sync_sessions::OpenTabsUIDelegate* TestSyncService::GetOpenTabsUIDelegate() {
  return nullptr;
}

UserShare* TestSyncService::GetUserShare() const {
  return nullptr;
}

syncer::SyncTokenStatus TestSyncService::GetSyncTokenStatus() const {
  return syncer::SyncTokenStatus();
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
  if (!custom_passphrase_enabled_) {
    // PASSWORDS are always encrypted.
    return ModelTypeSet(syncer::PASSWORDS);
  }
  // Some types can never be encrypted, e.g. DEVICE_INFO and
  // AUTOFILL_WALLET_DATA, so make sure we don't report them as encrypted.
  return syncer::Intersection(GetPreferredDataTypes(),
                              syncer::EncryptableUserTypes());
}

void TestSyncService::Shutdown() {}

}  // namespace syncer
