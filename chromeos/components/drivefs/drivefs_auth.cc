// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_auth.h"

#include "base/bind.h"
#include "components/account_id/account_id.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"

namespace drivefs {

namespace {
constexpr char kIdentityConsumerId[] = "drivefs";
}  // namespace

DriveFsAuth::DriveFsAuth(const base::Clock* clock,
                         const base::FilePath& profile_path,
                         Delegate* delegate)
    : clock_(clock), profile_path_(profile_path), delegate_(delegate) {}

DriveFsAuth::~DriveFsAuth() {}

base::Optional<std::string> DriveFsAuth::GetCachedAccessToken() {
  const auto& token = GetOrResetCachedToken(true);
  if (token.empty()) {
    return base::nullopt;
  }
  return token;
}

void DriveFsAuth::GetAccessToken(
    bool use_cached,
    mojom::DriveFsDelegate::GetAccessTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (get_access_token_callback_) {
    std::move(callback).Run(mojom::AccessTokenStatus::kTransientError, "");
    return;
  }

  const std::string& token = GetOrResetCachedToken(use_cached);
  if (!token.empty()) {
    std::move(callback).Run(mojom::AccessTokenStatus::kSuccess, token);
    return;
  }

  get_access_token_callback_ = std::move(callback);
  GetIdentityAccessor().GetPrimaryAccountWhenAvailable(
      base::BindOnce(&DriveFsAuth::AccountReady, base::Unretained(this)));
}

void DriveFsAuth::AccountReady(const AccountInfo& info,
                               const identity::AccountState& state) {
  GetIdentityAccessor().GetAccessToken(
      delegate_->GetAccountId().GetUserEmail(),
      {"https://www.googleapis.com/auth/drive"}, kIdentityConsumerId,
      base::BindOnce(&DriveFsAuth::GotChromeAccessToken,
                     base::Unretained(this)));
}

void DriveFsAuth::GotChromeAccessToken(
    const base::Optional<std::string>& access_token,
    base::Time expiration_time,
    const GoogleServiceAuthError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!access_token) {
    std::move(get_access_token_callback_)
        .Run(error.IsPersistentError()
                 ? mojom::AccessTokenStatus::kAuthError
                 : mojom::AccessTokenStatus::kTransientError,
             "");
    return;
  }
  UpdateCachedToken(*access_token, expiration_time);
  std::move(get_access_token_callback_)
      .Run(mojom::AccessTokenStatus::kSuccess, *access_token);
}

const std::string& DriveFsAuth::GetOrResetCachedToken(bool use_cached) {
  if (!use_cached || clock_->Now() >= last_token_expiry_) {
    last_token_.clear();
  }
  return last_token_;
}

void DriveFsAuth::UpdateCachedToken(const std::string& token,
                                    base::Time expiry) {
  last_token_ = token;
  last_token_expiry_ = expiry;
}

identity::mojom::IdentityAccessor& DriveFsAuth::GetIdentityAccessor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!identity_accessor_) {
    delegate_->GetConnector()->BindInterface(
        identity::mojom::kServiceName, mojo::MakeRequest(&identity_accessor_));
  }
  return *identity_accessor_;
}

}  // namespace drivefs
