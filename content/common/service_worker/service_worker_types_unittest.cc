// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "net/base/load_flags.h"

namespace content {

namespace {

using blink::mojom::FetchCacheMode;

TEST(ServiceWorkerFetchRequestTest, CacheModeTest) {
  EXPECT_EQ(FetchCacheMode::kDefault,
            ServiceWorkerFetchRequest::GetCacheModeFromLoadFlags(0));
  EXPECT_EQ(FetchCacheMode::kNoStore,
            ServiceWorkerFetchRequest::GetCacheModeFromLoadFlags(
                net::LOAD_DISABLE_CACHE));
  EXPECT_EQ(FetchCacheMode::kValidateCache,
            ServiceWorkerFetchRequest::GetCacheModeFromLoadFlags(
                net::LOAD_VALIDATE_CACHE));
  EXPECT_EQ(FetchCacheMode::kBypassCache,
            ServiceWorkerFetchRequest::GetCacheModeFromLoadFlags(
                net::LOAD_BYPASS_CACHE));
  EXPECT_EQ(FetchCacheMode::kForceCache,
            ServiceWorkerFetchRequest::GetCacheModeFromLoadFlags(
                net::LOAD_SKIP_CACHE_VALIDATION));
  EXPECT_EQ(FetchCacheMode::kOnlyIfCached,
            ServiceWorkerFetchRequest::GetCacheModeFromLoadFlags(
                net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION));
  EXPECT_EQ(FetchCacheMode::kUnspecifiedOnlyIfCachedStrict,
            ServiceWorkerFetchRequest::GetCacheModeFromLoadFlags(
                net::LOAD_ONLY_FROM_CACHE));
  EXPECT_EQ(FetchCacheMode::kUnspecifiedForceCacheMiss,
            ServiceWorkerFetchRequest::GetCacheModeFromLoadFlags(
                net::LOAD_ONLY_FROM_CACHE | net::LOAD_BYPASS_CACHE));
}

}  // namespace

}  // namespace content
