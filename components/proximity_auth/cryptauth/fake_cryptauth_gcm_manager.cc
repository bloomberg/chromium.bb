// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/cryptauth/fake_cryptauth_gcm_manager.h"

namespace proximity_auth {

FakeCryptAuthGCMManager::FakeCryptAuthGCMManager(
    const std::string& registration_id)
    : registration_in_progress_(false), registration_id_(registration_id) {}

FakeCryptAuthGCMManager::~FakeCryptAuthGCMManager() {}

void FakeCryptAuthGCMManager::StartListening() {}

void FakeCryptAuthGCMManager::RegisterWithGCM() {
  registration_in_progress_ = true;
}

std::string FakeCryptAuthGCMManager::GetRegistrationId() {
  return registration_id_;
}

void FakeCryptAuthGCMManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeCryptAuthGCMManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeCryptAuthGCMManager::CompleteRegistration(
    const std::string& registration_id) {
  DCHECK(registration_in_progress_);
  registration_in_progress_ = false;
  registration_id_ = registration_id;
  bool success = !registration_id_.empty();
  FOR_EACH_OBSERVER(Observer, observers_, OnGCMRegistrationResult(success));
}

void FakeCryptAuthGCMManager::PushReenrollMessage() {
  FOR_EACH_OBSERVER(Observer, observers_, OnReenrollMessage());
}

void FakeCryptAuthGCMManager::PushResyncMessage() {
  FOR_EACH_OBSERVER(Observer, observers_, OnResyncMessage());
}

}  // namespace proximity_auth
