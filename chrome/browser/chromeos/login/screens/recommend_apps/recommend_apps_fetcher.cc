// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher.h"

#include "base/callback.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/recommend_apps_fetcher_impl.h"

namespace chromeos {

namespace {

// The factory callback that will be used to create RecommendAppsFetcher
// instances other than default RecommendAppsFetcherImpl.
// It can be set by SetFactoryCallbackForTesting().
RecommendAppsFetcher::FactoryCallback* g_factory_callback = nullptr;

}  // namespace

// static
std::unique_ptr<RecommendAppsFetcher> RecommendAppsFetcher::Create(
    RecommendAppsScreenView* view) {
  if (g_factory_callback)
    return g_factory_callback->Run(view);
  return std::make_unique<RecommendAppsFetcherImpl>(view);
}

// static
void RecommendAppsFetcher::SetFactoryCallbackForTesting(
    FactoryCallback* callback) {
  DCHECK(!g_factory_callback || !callback);

  g_factory_callback = callback;
}

}  // namespace chromeos
