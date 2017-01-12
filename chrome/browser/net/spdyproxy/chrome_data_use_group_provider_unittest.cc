// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/chrome_data_use_group_provider.h"

#include <memory>

#include "base/memory/ref_counted.h"
#include "chrome/browser/net/spdyproxy/chrome_data_use_group.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ChromeDataUseGroupProviderTest : public testing::Test {
 protected:
  ChromeDataUseGroupProvider* data_use_group_provider() {
    return &data_use_group_provider_;
  }

  std::unique_ptr<net::URLRequest> CreateRequestForFrame(int render_process_id,
                                                         int render_frame_id) {
    std::unique_ptr<net::URLRequest> request = context_.CreateRequest(
        GURL("http://foo.com/"), net::IDLE, &test_delegate_);

    content::ResourceRequestInfo::AllocateForTesting(
        request.get(), content::RESOURCE_TYPE_MAIN_FRAME,
        /*ResourceContext=*/nullptr, render_process_id, /*render_view_id=*/-1,
        render_frame_id, /*is_main_frame=*/true,
        /*parent_is_main_frame=*/false,
        /*allow_download=*/true,
        /*is_async=*/true, content::PREVIEWS_OFF);

    return request;
  }

 private:
  // |thread_bundle_| must be the first field to ensure that threads are
  // constructed first and destroyed last.
  content::TestBrowserThreadBundle thread_bundle_;

  net::TestURLRequestContext context_;
  net::TestDelegate test_delegate_;
  ChromeDataUseGroupProvider data_use_group_provider_;
};

TEST_F(ChromeDataUseGroupProviderTest, SameMainFrame) {
  std::unique_ptr<net::URLRequest> request = CreateRequestForFrame(1, 1);
  scoped_refptr<data_reduction_proxy::DataUseGroup> group1 =
      data_use_group_provider()->GetDataUseGroup(request.get());

  request = CreateRequestForFrame(1, 1);
  scoped_refptr<data_reduction_proxy::DataUseGroup> group2 =
      data_use_group_provider()->GetDataUseGroup(request.get());

  EXPECT_EQ(group1, group2);
}

TEST_F(ChromeDataUseGroupProviderTest, DifferentMainFrame) {
  std::unique_ptr<net::URLRequest> request = CreateRequestForFrame(1, 1);
  scoped_refptr<data_reduction_proxy::DataUseGroup> group1 =
      data_use_group_provider()->GetDataUseGroup(request.get());

  request = CreateRequestForFrame(2, 2);
  scoped_refptr<data_reduction_proxy::DataUseGroup> group2 =
      data_use_group_provider()->GetDataUseGroup(request.get());

  EXPECT_NE(group1, group2);
}
