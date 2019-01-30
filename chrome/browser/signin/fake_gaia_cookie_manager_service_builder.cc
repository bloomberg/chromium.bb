// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

std::unique_ptr<KeyedService> BuildGaiaCookieManagerServiceWithURLLoader(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<GaiaCookieManagerService>(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile),
      base::BindRepeating(
          [](network::TestURLLoaderFactory* test_url_loader_factory)
              -> scoped_refptr<network::SharedURLLoaderFactory> {
            return test_url_loader_factory->GetSafeWeakWrapper();
          },
          test_url_loader_factory));
}

std::unique_ptr<KeyedService> BuildFakeGaiaCookieManagerServiceWithURLLoader(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<FakeGaiaCookieManagerService>(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      ChromeSigninClientFactory::GetForProfile(profile),
      test_url_loader_factory);
}
