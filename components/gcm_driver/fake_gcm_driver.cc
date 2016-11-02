// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/fake_gcm_driver.h"

#include "base/files/file_path.h"
#include "base/sequenced_task_runner.h"

namespace gcm {

FakeGCMDriver::FakeGCMDriver() : GCMDriver(base::FilePath(), nullptr) {
}

FakeGCMDriver::FakeGCMDriver(
    const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner)
    : GCMDriver(base::FilePath(), blocking_task_runner) {
}

FakeGCMDriver::~FakeGCMDriver() {
}

void FakeGCMDriver::OnSignedIn() {
}

void FakeGCMDriver::OnSignedOut() {
}

void FakeGCMDriver::AddConnectionObserver(GCMConnectionObserver* observer) {
}

void FakeGCMDriver::RemoveConnectionObserver(GCMConnectionObserver* observer) {
}

void FakeGCMDriver::Enable() {
}

void FakeGCMDriver::Disable() {
}

GCMClient* FakeGCMDriver::GetGCMClientForTesting() const {
  return NULL;
}

bool FakeGCMDriver::IsStarted() const {
  return true;
}

bool FakeGCMDriver::IsConnected() const {
  return true;
}

void FakeGCMDriver::GetGCMStatistics(const GetGCMStatisticsCallback& callback,
                                     ClearActivityLogs clear_logs) {
}

void FakeGCMDriver::SetGCMRecording(const GetGCMStatisticsCallback& callback,
                                    bool recording) {
}

GCMClient::Result FakeGCMDriver::EnsureStarted(
    GCMClient::StartMode start_mode) {
  return GCMClient::SUCCESS;
}

void FakeGCMDriver::RegisterImpl(const std::string& app_id,
                                 const std::vector<std::string>& sender_ids) {
}

void FakeGCMDriver::UnregisterImpl(const std::string& app_id) {
}

void FakeGCMDriver::SendImpl(const std::string& app_id,
                             const std::string& receiver_id,
                             const OutgoingMessage& message) {
}

void FakeGCMDriver::RecordDecryptionFailure(
    const std::string& app_id,
    GCMEncryptionProvider::DecryptionResult result) {
}

void FakeGCMDriver::SetAccountTokens(
    const std::vector<GCMClient::AccountTokenInfo>& account_tokens) {
}

void FakeGCMDriver::UpdateAccountMapping(
    const AccountMapping& account_mapping) {
}

void FakeGCMDriver::RemoveAccountMapping(const std::string& account_id) {
}

base::Time FakeGCMDriver::GetLastTokenFetchTime() {
  return base::Time();
}

void FakeGCMDriver::SetLastTokenFetchTime(const base::Time& time) {
}

void FakeGCMDriver::WakeFromSuspendForHeartbeat(bool wake) {
}

InstanceIDHandler* FakeGCMDriver::GetInstanceIDHandlerInternal() {
  return NULL;
}

void FakeGCMDriver::AddHeartbeatInterval(const std::string& scope,
                                         int interval_ms) {
}

void FakeGCMDriver::RemoveHeartbeatInterval(const std::string& scope) {
}

}  // namespace gcm
