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

void FakeGCMDriver::Enable() {
}

void FakeGCMDriver::Disable() {
}

void FakeGCMDriver::Register(const std::string& app_id,
                             const std::vector<std::string>& sender_ids,
                             const RegisterCallback& callback) {
}

void FakeGCMDriver::Unregister(const std::string& app_id,
                               const UnregisterCallback& callback) {
}

void FakeGCMDriver::Send(const std::string& app_id,
                         const std::string& receiver_id,
                         const GCMClient::OutgoingMessage& message,
                         const SendCallback& callback) {
}

GCMClient* FakeGCMDriver::GetGCMClientForTesting() const {
  return NULL;
}

bool FakeGCMDriver::IsStarted() const {
  return true;
}

bool FakeGCMDriver::IsGCMClientReady() const {
  return true;
}

void FakeGCMDriver::GetGCMStatistics(const GetGCMStatisticsCallback& callback,
                                     bool clear_logs) {
}

void FakeGCMDriver::SetGCMRecording(const GetGCMStatisticsCallback& callback,
                                    bool recording) {
}

std::string FakeGCMDriver::SignedInUserName() const {
  return std::string();
}

}  // namespace gcm
