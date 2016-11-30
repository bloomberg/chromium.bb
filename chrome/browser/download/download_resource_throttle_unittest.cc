// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_resource_throttle.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/download/mock_download_controller.h"
#endif

namespace {

const char kTestUrl[] = "http://www.example.com/";

}  // namespace

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() {}
  ~MockWebContentsDelegate() override {}
};

class MockResourceController : public content::ResourceController {
 public:
  MOCK_METHOD0(Cancel, void());
  MOCK_METHOD0(CancelAndIgnore, void());
  MOCK_METHOD1(CancelWithError, void(int));
  MOCK_METHOD0(Resume, void());
};

// Posts |quit_closure| to UI thread.
ACTION_P(QuitLoop, quit_closure) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   quit_closure);
}

class DownloadResourceThrottleTest : public ChromeRenderViewHostTestHarness {
 public:
  DownloadResourceThrottleTest()
      : throttle_(nullptr), limiter_(new DownloadRequestLimiter()) {
    // Cannot use IO_MAIN_LOOP with RenderViewHostTestHarness.
    SetThreadBundleOptions(content::TestBrowserThreadBundle::REAL_IO_THREAD);
  }

  ~DownloadResourceThrottleTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents()->SetDelegate(&delegate_);
    run_loop_.reset(new base::RunLoop());
#if BUILDFLAG(ANDROID_JAVA_UI)
    DownloadControllerBase::SetDownloadControllerBase(&download_controller_);
#endif
  }

  void TearDown() override {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       throttle_);
#if BUILDFLAG(ANDROID_JAVA_UI)
    DownloadControllerBase::SetDownloadControllerBase(nullptr);
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void StartThrottleOnIOThread(int process_id, int render_view_id) {
    throttle_ = new DownloadResourceThrottle(
        limiter_,
        base::Bind(&tab_util::GetWebContentsByID, process_id, render_view_id),
        GURL(kTestUrl), "GET");
    throttle_->set_controller_for_testing(&resource_controller_);
    bool defer;
    throttle_->WillStartRequest(&defer);
    EXPECT_EQ(true, defer);
  }

  void StartThrottle() {
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&DownloadResourceThrottleTest::StartThrottleOnIOThread,
                   base::Unretained(this),
                   web_contents()->GetRenderViewHost()->GetProcess()->GetID(),
                   web_contents()->GetRenderViewHost()->GetRoutingID()));
    run_loop_->Run();
  }

 protected:
  content::ResourceThrottle* throttle_;
  MockWebContentsDelegate delegate_;
  scoped_refptr<DownloadRequestLimiter> limiter_;
  ::testing::NiceMock<MockResourceController> resource_controller_;
  std::unique_ptr<base::RunLoop> run_loop_;
#if BUILDFLAG(ANDROID_JAVA_UI)
  chrome::android::MockDownloadController download_controller_;
#endif
};

TEST_F(DownloadResourceThrottleTest, StartDownloadThrottle_Basic) {
  EXPECT_CALL(resource_controller_, Resume())
      .WillOnce(QuitLoop(run_loop_->QuitClosure()));
  StartThrottle();
}

#if BUILDFLAG(ANDROID_JAVA_UI)
TEST_F(DownloadResourceThrottleTest, DownloadWithFailedFileAcecssRequest) {
  DownloadControllerBase::Get()
      ->SetApproveFileAccessRequestForTesting(false);
  EXPECT_CALL(resource_controller_, Cancel())
      .WillOnce(QuitLoop(run_loop_->QuitClosure()));
  StartThrottle();
}
#endif
