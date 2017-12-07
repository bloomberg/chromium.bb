// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/browser_download_service.h"

#include <vector>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/download/pass_kit_tab_helper.h"
#import "ios/web/public/download/download_controller.h"
#import "ios/web/public/download/download_task.h"
#include "ios/web/public/features.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
char kUrl[] = "https://test.test/";
char kMimeType[] = "application/vnd.apple.pkpass";

// Fake Tab Helper that substitutes real PassKitTabHelper for testing.
class StubPassKitTabHelper : public PassKitTabHelper {
 public:
  static void CreateForWebState(web::WebState* web_state) {
    web_state->SetUserData(
        UserDataKey(), base::WrapUnique(new StubPassKitTabHelper(web_state)));
  }

  // Adds the given task to tasks() lists.
  void Download(std::unique_ptr<web::DownloadTask> task) override {
    tasks_.push_back(std::move(task));
  }

  // Tasks added via Download() call.
  using DownloadTasks = std::vector<std::unique_ptr<web::DownloadTask>>;
  const DownloadTasks& tasks() const { return tasks_; }

 private:
  StubPassKitTabHelper(web::WebState* web_state)
      : PassKitTabHelper(web_state, /*delegate=*/nil) {}

  DownloadTasks tasks_;

  DISALLOW_COPY_AND_ASSIGN(StubPassKitTabHelper);
};

}  // namespace

// Test fixture for testing BrowserDownloadService class.
class BrowserDownloadServiceTest : public PlatformTest {
 protected:
  BrowserDownloadServiceTest()
      : browser_state_(browser_state_builder_.Build()) {
    feature_list_.InitAndEnableFeature(web::features::kNewPassKitDownload);
    StubPassKitTabHelper::CreateForWebState(&web_state_);

    // BrowserDownloadServiceFactory sets its service as
    // DownloadControllerDelegate. These test use separate
    // BrowserDownloadService, not created by factory. So delegate
    // is temporary removed for these tests to avoid DCHECKs.
    previous_delegate_ = download_controller()->GetDelegate();
    download_controller()->SetDelegate(nullptr);
    service_ = std::make_unique<BrowserDownloadService>(download_controller());
  }

  ~BrowserDownloadServiceTest() override {
    service_.reset();
    // Return back the original delegate so service created by service factory
    // can be destructed without DCHECKs.
    download_controller()->SetDelegate(previous_delegate_);
  }

  web::DownloadController* download_controller() {
    return web::DownloadController::FromBrowserState(browser_state_.get());
  }

  StubPassKitTabHelper* pass_kit_tab_helper() {
    return static_cast<StubPassKitTabHelper*>(
        PassKitTabHelper::FromWebState(&web_state_));
  }

  web::DownloadControllerDelegate* previous_delegate_;
  web::TestWebThreadBundle thread_bundle_;
  TestChromeBrowserState::Builder browser_state_builder_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<BrowserDownloadService> service_;
  web::TestWebState web_state_;
  base::test::ScopedFeatureList feature_list_;
};

// Tests that BrowserDownloadService downloads the task using
// PassKitTabHelper.
TEST_F(BrowserDownloadServiceTest, PkPassMimeType) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task = std::make_unique<web::FakeDownloadTask>(GURL(kUrl), kMimeType);
  web::DownloadTask* task_ptr = task.get();
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_EQ(1U, pass_kit_tab_helper()->tasks().size());
  EXPECT_EQ(task_ptr, pass_kit_tab_helper()->tasks()[0].get());
}

// Tests that BrowserDownloadService does not use PassKitTabHelper for pdf
// Mime Type.
TEST_F(BrowserDownloadServiceTest, PdfMimeType) {
  ASSERT_TRUE(download_controller()->GetDelegate());
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kUrl), "application/pdf");
  download_controller()->GetDelegate()->OnDownloadCreated(
      download_controller(), &web_state_, std::move(task));
  ASSERT_TRUE(pass_kit_tab_helper()->tasks().empty());
}
