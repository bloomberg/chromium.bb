// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/fake_sync_service.h"

namespace sync_driver {

FakeSyncService::FakeSyncService() : error_(GoogleServiceAuthError::NONE) {
}

FakeSyncService::~FakeSyncService() {
}

bool FakeSyncService::HasSyncSetupCompleted() const {
  return false;
}

bool FakeSyncService::IsSyncAllowed() const {
  return false;
}

bool FakeSyncService::IsSyncActive() const {
  return false;
}

syncer::ModelTypeSet FakeSyncService::GetActiveDataTypes() const {
  return syncer::ModelTypeSet();
}

void FakeSyncService::AddObserver(SyncServiceObserver* observer) {
}

void FakeSyncService::RemoveObserver(SyncServiceObserver* observer) {
}

bool FakeSyncService::HasObserver(const SyncServiceObserver* observer) const {
  return false;
}

bool FakeSyncService::CanSyncStart() const {
  return false;
}

void FakeSyncService::RequestStop(
    sync_driver::SyncService::SyncStopDataFate data_fate) {
}

void FakeSyncService::RequestStart() {
}

syncer::ModelTypeSet FakeSyncService::GetPreferredDataTypes() const {
  return syncer::ModelTypeSet();
}

void FakeSyncService::OnUserChoseDatatypes(bool sync_everything,
                                           syncer::ModelTypeSet chosen_types) {
}

void FakeSyncService::SetSyncSetupCompleted() {
}

bool FakeSyncService::FirstSetupInProgress() const {
  return false;
}

void FakeSyncService::SetSetupInProgress(bool setup_in_progress) {
}

bool FakeSyncService::setup_in_progress() const {
  return false;
}

bool FakeSyncService::ConfigurationDone() const {
  return false;
}

const GoogleServiceAuthError& FakeSyncService::GetAuthError() const {
  return error_;
}

bool FakeSyncService::HasUnrecoverableError() const {
  return false;
}

bool FakeSyncService::backend_initialized() const {
  return false;
}

OpenTabsUIDelegate* FakeSyncService::GetOpenTabsUIDelegate() {
  return nullptr;
}

bool FakeSyncService::IsPassphraseRequiredForDecryption() const {
  return false;
}

base::Time FakeSyncService::GetExplicitPassphraseTime() const {
  return base::Time();
}

bool FakeSyncService::IsUsingSecondaryPassphrase() const {
  return false;
}

void FakeSyncService::EnableEncryptEverything() {
}

void FakeSyncService::SetEncryptionPassphrase(const std::string& passphrase,
                                              PassphraseType type) {
}

bool FakeSyncService::SetDecryptionPassphrase(const std::string& passphrase) {
  return false;
}

bool FakeSyncService::IsPassphraseRequired() const {
  return false;
}

syncer::ModelTypeSet FakeSyncService::GetEncryptedDataTypes() const {
  return syncer::ModelTypeSet();
}

}  // namespace sync_driver
