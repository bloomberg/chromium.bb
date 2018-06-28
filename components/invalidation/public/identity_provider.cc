// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/public/identity_provider.h"

namespace invalidation {

IdentityProvider::Observer::~Observer() {}

IdentityProvider::~IdentityProvider() {}

void IdentityProvider::AddObserver(Observer* observer) {
  // See the comment on |num_observers_| in the .h file for why this addition
  // must happen here.
  if (num_observers_ == 0) {
    OAuth2TokenService* token_service = GetTokenService();
    if (token_service)
      token_service->AddObserver(this);
  }

  num_observers_++;
  observers_.AddObserver(observer);
}

void IdentityProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
  num_observers_--;

  // See the comment on |num_observers_| in the .h file for why this removal
  // must happen here.
  if (num_observers_ == 0) {
    OAuth2TokenService* token_service = GetTokenService();
    if (token_service)
      token_service->RemoveObserver(this);
  }
}

void IdentityProvider::OnRefreshTokenAvailable(const std::string& account_id) {
  if (account_id != GetActiveAccountId())
    return;
  for (auto& observer : observers_)
    observer.OnActiveAccountRefreshTokenUpdated();
}

void IdentityProvider::OnRefreshTokenRevoked(const std::string& account_id) {
  if (account_id != GetActiveAccountId())
    return;
  for (auto& observer : observers_)
    observer.OnActiveAccountRefreshTokenRemoved();
}

IdentityProvider::IdentityProvider() : num_observers_(0) {}

void IdentityProvider::FireOnActiveAccountLogin() {
  for (auto& observer : observers_)
    observer.OnActiveAccountLogin();
}

void IdentityProvider::FireOnActiveAccountLogout() {
  for (auto& observer : observers_)
    observer.OnActiveAccountLogout();
}

}  // namespace invalidation
