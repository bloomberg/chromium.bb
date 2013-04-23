// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_update_engine_client.h"

namespace chromeos {

FakeUpdateEngineClient::FakeUpdateEngineClient() {
}

FakeUpdateEngineClient::~FakeUpdateEngineClient() {
}

void FakeUpdateEngineClient::AddObserver(Observer* observer) {
}

void FakeUpdateEngineClient::RemoveObserver(Observer* observer) {
}

bool FakeUpdateEngineClient::HasObserver(Observer* observer) {
  return false;
}

void FakeUpdateEngineClient::RequestUpdateCheck(
    const UpdateCheckCallback& callback) {
}

void FakeUpdateEngineClient::RebootAfterUpdate() {
}

void FakeUpdateEngineClient::SetReleaseTrack(const std::string& track) {
}

void FakeUpdateEngineClient::GetReleaseTrack(
    const GetReleaseTrackCallback& callback) {
}

FakeUpdateEngineClient::Status FakeUpdateEngineClient::GetLastStatus() {
  return update_engine_client_status_;
}

void FakeUpdateEngineClient::set_update_engine_client_status(
    const UpdateEngineClient::Status& status) {
  update_engine_client_status_ = status;
}

}  // namespace chromeos
