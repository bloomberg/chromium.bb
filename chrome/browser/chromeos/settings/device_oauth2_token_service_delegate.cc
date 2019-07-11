// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service_delegate.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

DeviceOAuth2TokenServiceDelegate::DeviceOAuth2TokenServiceDelegate(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    DeviceOAuth2TokenService* service)
    : url_loader_factory_(url_loader_factory),
      service_(service) {}

DeviceOAuth2TokenServiceDelegate::~DeviceOAuth2TokenServiceDelegate() = default;

scoped_refptr<network::SharedURLLoaderFactory>
DeviceOAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return url_loader_factory_;
}

std::unique_ptr<OAuth2AccessTokenFetcher>
DeviceOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  std::string refresh_token = service_->GetRefreshToken();
  DCHECK(!refresh_token.empty());
  return std::make_unique<OAuth2AccessTokenFetcherImpl>(
      consumer, url_loader_factory, refresh_token);
}

}  // namespace chromeos
