// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/ui/ntp/google_landing_controller.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/test_web_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class GoogleLandingControllerTest : public BlockCleanupTest {
 public:
  GoogleLandingControllerTest()
      : ui_thread_(web::WebThread::UI, &message_loop_),
        io_thread_(web::WebThread::IO, &message_loop_) {}

 protected:
  void SetUp() override {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        IOSChromeTabRestoreServiceFactory::GetInstance(),
        IOSChromeTabRestoreServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());

    chrome_browser_state_ = test_cbs_builder.Build();
    BlockCleanupTest::SetUp();

    // Set up tab restore service.
    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForBrowserState(
            chrome_browser_state_.get());
    template_url_service->Load();

    // Set up stub UrlLoader.
    mockUrlLoader_ = [OCMockObject mockForProtocol:@protocol(UrlLoader)];
    controller_ = [[GoogleLandingController alloc]
            initWithLoader:(id<UrlLoader>)mockUrlLoader_
              browserState:chrome_browser_state_.get()
                   focuser:nil
        webToolbarDelegate:nil
                  tabModel:nil];
  };

  base::MessageLoopForUI message_loop_;
  web::TestWebThread ui_thread_;
  web::TestWebThread io_thread_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  OCMockObject* mockUrlLoader_;
  GoogleLandingController* controller_;
};

TEST_F(GoogleLandingControllerTest, TestConstructorDestructor) {
  EXPECT_TRUE(controller_);
}

}  // anonymous namespace
