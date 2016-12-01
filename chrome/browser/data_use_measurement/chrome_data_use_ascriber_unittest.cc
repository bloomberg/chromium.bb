// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_use_measurement/chrome_data_use_ascriber.h"

#include <list>
#include <memory>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "components/data_use_measurement/core/data_use_recorder.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/process_type.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
int kRenderProcessId = 1;
int kRenderFrameId = 2;
int kRequestId = 3;
}

namespace data_use_measurement {

class ChromeDataUseAscriberTest : public testing::Test {
 protected:
  ChromeDataUseAscriberTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        resource_context_(new content::MockResourceContext(&context_)) {}

  void SetUp() override {}

  void TearDown() override { recorders().clear(); }

  std::list<ChromeDataUseRecorder>& recorders() {
    return ascriber_.data_use_recorders_;
  }

  net::TestURLRequestContext* context() { return &context_; }

  content::MockResourceContext* resource_context() {
    return resource_context_.get();
  }

  ChromeDataUseAscriber* ascriber() { return &ascriber_; }

  std::unique_ptr<net::URLRequest> CreateNewRequest(std::string url,
                                                    bool is_main_frame,
                                                    int request_id,
                                                    int render_process_id,
                                                    int render_frame_id) {
    std::unique_ptr<net::URLRequest> request =
        context()->CreateRequest(GURL(url), net::IDLE, nullptr);
    // TODO(kundaji): Allow request_id to be specified in AllocateForTesting.
    content::ResourceRequestInfo::AllocateForTesting(
        request.get(),
        content::RESOURCE_TYPE_MAIN_FRAME,
        resource_context(),
        render_process_id,
        -1,  // render_view_id
        render_frame_id,
        is_main_frame,
        false,   // parent_is_main_frame
        false,   // allow_download
        true,    // is_async
        false);  // is_using_lofi
    return request;
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  ChromeDataUseAscriber ascriber_;
  net::TestURLRequestContext context_;
  std::unique_ptr<content::MockResourceContext> resource_context_;
};

TEST_F(ChromeDataUseAscriberTest, NoRecorderWithoutFrame) {
  if (content::IsBrowserSideNavigationEnabled())
    return;

  std::unique_ptr<net::URLRequest> request = CreateNewRequest(
      "http://test.com", true, kRequestId, kRenderProcessId, kRenderFrameId);

  // Main frame request should not cause a recorder to be created, since the
  // frame does not exist.
  ascriber()->OnBeforeUrlRequest(request.get());
  EXPECT_EQ(0u, recorders().size());

  // Frame is created.
  ascriber()->RenderFrameCreated(kRenderProcessId, kRenderFrameId, -1, -1);
  EXPECT_EQ(1u, recorders().size());

  // Request should cause a recorder to be created.
  ascriber()->OnBeforeUrlRequest(request.get());
  EXPECT_EQ(2u, recorders().size());
}

}  // namespace data_use_measurement
