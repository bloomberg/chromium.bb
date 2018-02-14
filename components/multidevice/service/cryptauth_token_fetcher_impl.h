// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MULTIDEVICE_CRYPTAUTH_ACCESS_TOKEN_FETCHER_IMPL
#define COMPONENTS_MULTIDEVICE_CRYPTAUTH_ACCESS_TOKEN_FETCHER_IMPL

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/cryptauth/cryptauth_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace identity {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace identity

namespace multidevice {

// CryptAuthAccessTokenFetcher implementation which utilizes IdentityManager.
class CryptAuthAccessTokenFetcherImpl
    : public cryptauth::CryptAuthAccessTokenFetcher {
 public:
  CryptAuthAccessTokenFetcherImpl(identity::IdentityManager* identity_manager);

  ~CryptAuthAccessTokenFetcherImpl() override;

  // cryptauth::CryptAuthAccessTokenFetcher:
  void FetchAccessToken(const AccessTokenCallback& callback) override;

 private:
  void InvokeThenClearPendingCallbacks(const std::string& access_token);
  void OnAccessTokenFetched(const GoogleServiceAuthError& error,
                            const std::string& access_token);

  identity::IdentityManager* identity_manager_;

  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::vector<AccessTokenCallback> pending_callbacks_;

  base::WeakPtrFactory<CryptAuthAccessTokenFetcherImpl> weak_ptr_factory_;
};

}  // namespace multidevice

#endif  // COMPONENTS_MULTIDEVICE_CRYPTAUTH_ACCESS_TOKEN_FETCHER_IMPL
