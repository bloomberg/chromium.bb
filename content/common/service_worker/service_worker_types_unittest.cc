// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types.h"
#include "base/guid.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"
#include "url/mojom/url_gurl_mojom_traits.h"

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

TEST(ServiceWorkerRequestTest, SerialiazeDeserializeRoundTrip) {
  ServiceWorkerFetchRequest request(
      GURL("foo.com"), "GET", {{"User-Agent", "Chrome"}},
      Referrer(
          GURL("bar.com"),
          blink::WebReferrerPolicy::kWebReferrerPolicyNoReferrerWhenDowngrade),
      true);
  request.mode = network::mojom::FetchRequestMode::kSameOrigin;
  request.is_main_resource_load = true;
  request.request_context_type = blink::mojom::RequestContextType::IFRAME;
  request.credentials_mode = network::mojom::FetchCredentialsMode::kSameOrigin;
  request.cache_mode = blink::mojom::FetchCacheMode::kForceCache;
  request.redirect_mode = network::mojom::FetchRedirectMode::kManual;
  request.integrity = "integrity";
  request.keepalive = true;
  request.client_id = "42";

  EXPECT_EQ(request.Serialize(),
            ServiceWorkerFetchRequest::ParseFromString(request.Serialize())
                .Serialize());
}

}  // namespace

}  // namespace content
