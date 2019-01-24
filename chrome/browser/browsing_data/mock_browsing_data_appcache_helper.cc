// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_appcache_helper.h"

#include <vector>

#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"

MockBrowsingDataAppCacheHelper::MockBrowsingDataAppCacheHelper(
    content::BrowserContext* browser_context)
    : BrowsingDataAppCacheHelper(browser_context),
      response_(new content::AppCacheInfoCollection) {
}

MockBrowsingDataAppCacheHelper::~MockBrowsingDataAppCacheHelper() {
}

void MockBrowsingDataAppCacheHelper::StartFetching(
    FetchCallback completion_callback) {
  ASSERT_FALSE(completion_callback.is_null());
  ASSERT_TRUE(completion_callback_.is_null());
  completion_callback_ = std::move(completion_callback);
}

void MockBrowsingDataAppCacheHelper::DeleteAppCacheGroup(
    const GURL& manifest_url) {
}

void MockBrowsingDataAppCacheHelper::AddAppCacheSamples() {
  const GURL kOriginURL("http://hello/");
  const url::Origin kOrigin(url::Origin::Create(kOriginURL));
  blink::mojom::AppCacheInfo mock_manifest_1;
  blink::mojom::AppCacheInfo mock_manifest_2;
  blink::mojom::AppCacheInfo mock_manifest_3;
  mock_manifest_1.manifest_url = kOriginURL.Resolve("manifest1");
  mock_manifest_1.size = 1;
  mock_manifest_2.manifest_url = kOriginURL.Resolve("manifest2");
  mock_manifest_2.size = 2;
  mock_manifest_3.manifest_url = kOriginURL.Resolve("manifest3");
  mock_manifest_3.size = 3;
  std::vector<blink::mojom::AppCacheInfo> info_vector;
  info_vector.push_back(mock_manifest_1);
  info_vector.push_back(mock_manifest_2);
  info_vector.push_back(mock_manifest_3);
  response_->infos_by_origin[kOrigin] = info_vector;
}

void MockBrowsingDataAppCacheHelper::Notify() {
  std::move(completion_callback_).Run(response_);
}
