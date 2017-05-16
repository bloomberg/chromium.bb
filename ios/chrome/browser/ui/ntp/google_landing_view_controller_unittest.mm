// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/ui/ntp/google_landing_mediator.h"
#import "ios/chrome/browser/ui/ntp/google_landing_view_controller.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class GoogleLandingViewControllerTest : public BlockCleanupTest {
 public:
  GoogleLandingViewControllerTest() = default;

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
    controller_ = [[GoogleLandingViewController alloc] init];
    webStateList_ = base::MakeUnique<WebStateList>(&webStateListDelegate_);
    mediator_ = [[GoogleLandingMediator alloc]
        initWithConsumer:controller_
            browserState:chrome_browser_state_.get()
              dispatcher:nil
            webStateList:webStateList_.get()];
  };

  void TearDown() override { [mediator_ shutdown]; }

  web::TestWebThreadBundle test_web_thread_bundle_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  FakeWebStateListDelegate webStateListDelegate_;
  std::unique_ptr<WebStateList> webStateList_;
  OCMockObject* mockUrlLoader_;
  GoogleLandingMediator* mediator_;
  GoogleLandingViewController* controller_;
};

TEST_F(GoogleLandingViewControllerTest, TestConstructorDestructor) {
  EXPECT_TRUE(controller_);
}

}  // anonymous namespace
