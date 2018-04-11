// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/fake_lock_handler.h"

namespace proximity_auth {

FakeLockHandler::FakeLockHandler() {}

FakeLockHandler::~FakeLockHandler() {}

void FakeLockHandler::ShowBannerMessage(const base::string16& message,
                                        bool is_warning) {}

void FakeLockHandler::ShowUserPodCustomIcon(
    const AccountId& account_id,
    const ScreenlockBridge::UserPodCustomIconOptions& icon) {}

void FakeLockHandler::HideUserPodCustomIcon(const AccountId& account_id) {}

void FakeLockHandler::EnableInput() {}

void FakeLockHandler::SetAuthType(const AccountId& account_id,
                                  mojom::AuthType auth_type,
                                  const base::string16& auth_value) {}

mojom::AuthType FakeLockHandler::GetAuthType(
    const AccountId& account_id) const {
  return mojom::AuthType::USER_CLICK;
}

FakeLockHandler::ScreenType FakeLockHandler::GetScreenType() const {
  return FakeLockHandler::LOCK_SCREEN;
}

void FakeLockHandler::Unlock(const AccountId& account_id) {}

void FakeLockHandler::AttemptEasySignin(const AccountId& account_id,
                                        const std::string& secret,
                                        const std::string& key_label) {}

}  // namespace proximity_auth
