// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_cross_origin_filter.h"

#include "base/macros.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/common/service_worker/service_worker_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const char kFirstOrigin[] = "https://example.com";
const char kFirstOriginFile[] = "https://example.com/cat.jpg";
const char kSecondOriginFile[] = "https://chrome.com/cat.jpg";

const char kAccessControlAllowOriginHeader[] = "access-control-allow-origin";

class BackgroundFetchCrossOriginFilterTest : public ::testing::Test {
 public:
  BackgroundFetchCrossOriginFilterTest()
      : source_(url::Origin(GURL(kFirstOrigin))) {}
  ~BackgroundFetchCrossOriginFilterTest() override = default;

  // Creates a BackgroundFetchRequestInfo instance filled with the information
  // required for the cross-origin filter to work. This method takes a
  // |response_url| and an initializer list for key => value header pairs.
  scoped_refptr<BackgroundFetchRequestInfo> CreateRequestInfo(
      const char* response_url,
      std::initializer_list<
          typename std::map<std::string, std::string>::value_type>
          response_headers) {
    scoped_refptr<BackgroundFetchRequestInfo> request_info =
        make_scoped_refptr(new BackgroundFetchRequestInfo(
            0 /* request_info */, ServiceWorkerFetchRequest()));

    request_info->download_state_populated_ = true;
    request_info->response_headers_ = response_headers;

    request_info->response_data_populated_ = true;
    request_info->url_chain_ = {GURL(response_url)};

    return request_info;
  }

 protected:
  url::Origin source_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundFetchCrossOriginFilterTest);
};

TEST_F(BackgroundFetchCrossOriginFilterTest, SameOrigin) {
  auto same_origin_response = CreateRequestInfo(kFirstOriginFile, {});

  BackgroundFetchCrossOriginFilter filter(source_, *same_origin_response);
  EXPECT_TRUE(filter.CanPopulateBody());
}

TEST_F(BackgroundFetchCrossOriginFilterTest, CrossOrigin) {
  auto cross_origin_response = CreateRequestInfo(kSecondOriginFile, {});

  BackgroundFetchCrossOriginFilter filter(source_, *cross_origin_response);
  EXPECT_FALSE(filter.CanPopulateBody());
}

TEST_F(BackgroundFetchCrossOriginFilterTest, CrossOriginAllowAllOrigins) {
  auto cross_origin_response = CreateRequestInfo(
      kSecondOriginFile, {{kAccessControlAllowOriginHeader, "*"}});

  BackgroundFetchCrossOriginFilter filter(source_, *cross_origin_response);
  EXPECT_TRUE(filter.CanPopulateBody());
}

TEST_F(BackgroundFetchCrossOriginFilterTest, CrossOriginAllowSpecificOrigin) {
  // With a single origin in the Access-Control-Allow-Origin header.
  {
    auto cross_origin_response = CreateRequestInfo(
        kSecondOriginFile, {{kAccessControlAllowOriginHeader, kFirstOrigin}});

    BackgroundFetchCrossOriginFilter filter(source_, *cross_origin_response);
    EXPECT_TRUE(filter.CanPopulateBody());
  }

  // With multiple origins in the Access-Control-Allow-Origin header.
  {
    const char kOriginList[] =
        "https://foo.com, https://example.com, https://bar.com:1500";

    auto cross_origin_response = CreateRequestInfo(
        kSecondOriginFile, {{kAccessControlAllowOriginHeader, kOriginList}});

    BackgroundFetchCrossOriginFilter filter(source_, *cross_origin_response);
    EXPECT_TRUE(filter.CanPopulateBody());
  }

  // With an invalid value included in the Access-Control-Allow-Origin header.
  {
    const char kOriginList[] =
        "https://foo.com, https://example.com, INVALID, https://bar.com:1500";

    auto cross_origin_response = CreateRequestInfo(
        kSecondOriginFile, {{kAccessControlAllowOriginHeader, kOriginList}});

    BackgroundFetchCrossOriginFilter filter(source_, *cross_origin_response);
    EXPECT_FALSE(filter.CanPopulateBody());
  }
}

}  // namespace content
