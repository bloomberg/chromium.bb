// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CONTEXT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CONTEXT_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "google_apis/gaia/ubertoken_fetcher.h"

class Profile;
class ProfileOAuth2TokenService;

namespace content {
class StoragePartition;
}

namespace arc {

class ArcAuthContextDelegate;

class ArcAuthContext : public UbertokenConsumer,
                       public GaiaAuthConsumer,
                       public OAuth2TokenService::Observer {
 public:
  ArcAuthContext(ArcAuthContextDelegate* delegate, Profile* profile);
  ~ArcAuthContext() override;

  void PrepareContext();

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;

  // UbertokenConsumer:
  void OnUbertokenSuccess(const std::string& token) override;
  void OnUbertokenFailure(const GoogleServiceAuthError& error) override;

  // GaiaAuthConsumer:
  void OnMergeSessionSuccess(const std::string& data) override;
  void OnMergeSessionFailure(const GoogleServiceAuthError& error) override;

  const std::string& account_id() const { return account_id_; }

  ProfileOAuth2TokenService* token_service() { return token_service_; }

 private:
  void StartFetchers();
  void OnRefreshTokenTimeout();

  // Unowned pointers.
  ArcAuthContextDelegate* const delegate_;
  ProfileOAuth2TokenService* token_service_;

  bool context_prepared_ = false;

  // Owned by content::BrowserContent. Used to isolate cookies for auth server
  // communication and shared with Arc OptIn UI platform app.
  content::StoragePartition* storage_partition_ = nullptr;

  std::string account_id_;
  std::unique_ptr<GaiaAuthFetcher> merger_fetcher_;
  std::unique_ptr<UbertokenFetcher> ubertoken_fetcher_;
  base::OneShotTimer refresh_token_timeout_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthContext);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CONTEXT_H_
