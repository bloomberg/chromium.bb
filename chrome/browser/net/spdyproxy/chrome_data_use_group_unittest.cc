// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/chrome_data_use_group.h"

#include <memory>

#include "base/run_loop.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/previews_state.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class ChromeDataUseGroupTest : public testing::Test {
 protected:
  void SetUp() override {
    std::unique_ptr<net::URLRequest> request = context_.CreateRequest(
        GURL("http://foo.com/index.html"), net::IDLE, &test_delegate_);

    content::ResourceRequestInfo::AllocateForTesting(
        request.get(), content::RESOURCE_TYPE_MAIN_FRAME, nullptr,
        /*render_process_id=*/1,
        /*render_view_id=*/-1,
        /*render_frame_id=*/1,
        /*is_main_frame=*/true,
        /*parent_is_main_frame=*/false,
        /*allow_download=*/true,
        /*is_async=*/true, content::PREVIEWS_OFF);

    data_use_group_ = new ChromeDataUseGroup(request.get());
    base::RunLoop().RunUntilIdle();
  }

  ChromeDataUseGroup* data_use_group() { return data_use_group_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  net::TestURLRequestContext context_;
  net::TestDelegate test_delegate_;
  scoped_refptr<ChromeDataUseGroup> data_use_group_;
};

TEST_F(ChromeDataUseGroupTest, GetHostName) {
  EXPECT_EQ("foo.com", data_use_group()->GetHostname());
}

TEST_F(ChromeDataUseGroupTest, GetHostNameExplicitInitialize) {
  data_use_group()->Initialize();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("foo.com", data_use_group()->GetHostname());
}
