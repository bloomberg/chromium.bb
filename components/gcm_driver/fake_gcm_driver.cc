// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/fake_gcm_driver.h"

namespace gcm {

FakeGCMDriver::FakeGCMDriver() {
}

FakeGCMDriver::~FakeGCMDriver() {
}

void FakeGCMDriver::Shutdown() {
}

void FakeGCMDriver::AddAppHandler(
    const std::string& app_id, GCMAppHandler* handler) {
}

void FakeGCMDriver::RemoveAppHandler(const std::string& app_id) {
}

void FakeGCMDriver::OnSignedIn() {
}

void FakeGCMDriver::Purge() {
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
                                     bool clear_logs) {
}

void FakeGCMDriver::SetGCMRecording(const GetGCMStatisticsCallback& callback,
                                    bool recording) {
}

GCMClient::Result FakeGCMDriver::EnsureStarted() {
  return GCMClient::SUCCESS;
}

void FakeGCMDriver::RegisterImpl(const std::string& app_id,
                                 const std::vector<std::string>& sender_ids) {
}

void FakeGCMDriver::UnregisterImpl(const std::string& app_id) {
}

void FakeGCMDriver::SendImpl(const std::string& app_id,
                             const std::string& receiver_id,
                             const GCMClient::OutgoingMessage& message) {
}

}  // namespace gcm
