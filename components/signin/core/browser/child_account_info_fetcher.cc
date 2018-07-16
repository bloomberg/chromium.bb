// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/child_account_info_fetcher.h"

#include "build/build_config.h"

#include "services/network/public/cpp/shared_url_loader_factory.h"
#if defined(OS_ANDROID)
#include "components/signin/core/browser/child_account_info_fetcher_android.h"
#else
#include "components/signin/core/browser/child_account_info_fetcher_impl.h"
#endif

// static
std::unique_ptr<ChildAccountInfoFetcher> ChildAccountInfoFetcher::CreateFrom(
    const std::string& account_id,
    AccountFetcherService* fetcher_service,
    OAuth2TokenService* token_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    invalidation::InvalidationService* invalidation_service) {
#if defined(OS_ANDROID)
  return ChildAccountInfoFetcherAndroid::Create(fetcher_service, account_id);
#else
  return std::make_unique<ChildAccountInfoFetcherImpl>(
      account_id, fetcher_service, token_service, url_loader_factory,
      invalidation_service);
#endif
}

ChildAccountInfoFetcher::~ChildAccountInfoFetcher() {
}

void ChildAccountInfoFetcher::InitializeForTests() {
#if defined(OS_ANDROID)
  ChildAccountInfoFetcherAndroid::InitializeForTests();
#endif
}
