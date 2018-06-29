// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/identity_provider.h"

namespace invalidation {

IdentityProvider::Observer::~Observer() {}

IdentityProvider::~IdentityProvider() {}

void IdentityProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void IdentityProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

IdentityProvider::IdentityProvider() {}

void IdentityProvider::ProcessRefreshTokenUpdateForAccount(
    const std::string& account_id) {
  if (account_id != GetActiveAccountId())
    return;
  for (auto& observer : observers_)
    observer.OnActiveAccountRefreshTokenUpdated();
}

void IdentityProvider::ProcessRefreshTokenRemovalForAccount(
    const std::string& account_id) {
  if (account_id != GetActiveAccountId())
    return;
  for (auto& observer : observers_)
    observer.OnActiveAccountRefreshTokenRemoved();
}

void IdentityProvider::FireOnActiveAccountLogin() {
  for (auto& observer : observers_)
    observer.OnActiveAccountLogin();
}

void IdentityProvider::FireOnActiveAccountLogout() {
  for (auto& observer : observers_)
    observer.OnActiveAccountLogout();
}

}  // namespace invalidation
