// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_service.h"

#import <Foundation/Foundation.h>
#import <PassKit/PassKit.h>

#include <memory>

#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/url_loading/app_url_loading_service.h"
#import "ios/chrome/browser/url_loading/test_app_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_state_list_web_usage_enabler_factory.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ULSTestTabMock : OCMockComplexTypeHelper {
  web::WebState* _webState;
}

@property(nonatomic, assign) web::WebState* webState;
@end

@implementation ULSTestTabMock
- (web::WebState*)webState {
  return _webState;
}
- (void)setWebState:(web::WebState*)webState {
  _webState = webState;
}
@end

@interface ULSTestTabModel : OCMockComplexTypeHelper
- (instancetype)init NS_DESIGNATED_INITIALIZER;
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
@property(nonatomic, readonly) WebStateList* webStateList;
@end

@implementation ULSTestTabModel {
  FakeWebStateListDelegate _webStateListDelegate;
  std::unique_ptr<WebStateList> _webStateList;
}
@synthesize browserState = _browserState;

- (instancetype)init {
  if ((self = [super
           initWithRepresentedObject:[OCMockObject
                                         niceMockForClass:[TabModel class]]])) {
    _webStateList = std::make_unique<WebStateList>(&_webStateListDelegate);
  }
  return self;
}

- (WebStateList*)webStateList {
  return _webStateList.get();
}
@end

@interface URLLoadingServiceTestDelegate : NSObject <URLLoadingServiceDelegate>
@end

@implementation URLLoadingServiceTestDelegate

- (instancetype)init {
  self = [super init];
  return self;
}

#pragma mark - URLLoadingServiceDelegate

- (void)openURLInNewTabWithCommand:(OpenNewTabCommand*)command {
}

- (void)animateOpenBackgroundTabFromParams:(UrlLoadParams*)params
                                completion:(void (^)())completion {
}

@end

#pragma mark -

namespace {
class URLLoadingServiceTest : public BlockCleanupTest {
 public:
  URLLoadingServiceTest() {}

 protected:
  void SetUp() override {
    BlockCleanupTest::SetUp();
    // Set up a TestChromeBrowserState instance.
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    // Set up mock TabModel and Tab.
    id tabModel = [[ULSTestTabModel alloc] init];
    [tabModel setBrowserState:chrome_browser_state_.get()];

    // Enable web usage for the mock TabModel's WebStateList.
    WebStateListWebUsageEnabler* enabler =
        WebStateListWebUsageEnablerFactory::GetInstance()->GetForBrowserState(
            chrome_browser_state_.get());
    enabler->SetWebStateList([tabModel webStateList]);
    enabler->SetWebUsageEnabled(true);

    tabModel_ = tabModel;

    // Stub methods for TabModel.
    NSUInteger tabCount = 1;
    [[[tabModel stub] andReturnValue:OCMOCK_VALUE(tabCount)] count];
    id currentTab = [[ULSTestTabMock alloc]
        initWithRepresentedObject:[OCMockObject niceMockForClass:[Tab class]]];
    [[[tabModel stub] andReturn:currentTab] currentTab];
    [[[tabModel stub] andReturn:currentTab] tabAtIndex:0];
    [[tabModel stub] addObserver:[OCMArg any]];
    [[tabModel stub] removeObserver:[OCMArg any]];
    [[tabModel stub] saveSessionImmediately:NO];
    [[tabModel stub] setCurrentTab:[OCMArg any]];
    [[tabModel stub] closeAllTabs];
    tab_ = currentTab;

    web::WebState::CreateParams params(chrome_browser_state_.get());
    std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
    webState_.reset(static_cast<web::WebState*>(webState.release()));
    [currentTab setWebState:webState_.get()];

    browser_ = new TestBrowser(chrome_browser_state_.get(), tabModel);

    app_service_ = new TestAppUrlLoadingService();

    url_loading_delegate_ = [[URLLoadingServiceTestDelegate alloc] init];
    service_ = UrlLoadingServiceFactory::GetForBrowserState(
        chrome_browser_state_.get());
    service_->SetDelegate(url_loading_delegate_);
    service_->SetBrowser(browser_);
    service_->SetAppService(app_service_);
  }

  void TearDown() override {
    // Cleanup to avoid debugger crash in non empty observer lists.
    WebStateList* web_state_list = tabModel_.webStateList;
    web_state_list->CloseAllWebStates(
        WebStateList::ClosingFlags::CLOSE_NO_FLAGS);

    BlockCleanupTest::TearDown();
  }

  // Returns a new unique_ptr containing a test webstate.
  std::unique_ptr<web::TestWebState> CreateTestWebState() {
    auto web_state = std::make_unique<web::TestWebState>();
    web_state->SetBrowserState(chrome_browser_state_.get());
    web_state->SetNavigationManager(
        std::make_unique<web::TestNavigationManager>());
    return web_state;
  }

  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<web::WebState> webState_;
  Tab* tab_;
  TabModel* tabModel_;
  Browser* browser_;
  URLLoadingServiceTestDelegate* url_loading_delegate_;
  UrlLoadingService* service_;
  TestAppUrlLoadingService* app_service_;
};

TEST_F(URLLoadingServiceTest, TestSwitchToTab) {
  WebStateList* web_state_list = tabModel_.webStateList;
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::TestWebState> web_state = CreateTestWebState();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetCurrentURL(GURL("http://test/1"));
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  std::unique_ptr<web::TestWebState> web_state_2 = CreateTestWebState();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  GURL url("http://test/2");
  web_state_2->SetCurrentURL(url);
  web_state_list->InsertWebState(1, std::move(web_state_2),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  web_state_list->ActivateWebStateAt(0);

  ASSERT_EQ(web_state_ptr, web_state_list->GetActiveWebState());

  service_->OpenUrl(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests that switch to open tab from the NTP close it if it doesn't have
// navigation history.
TEST_F(URLLoadingServiceTest, TestSwitchToTabFromNTP) {
  WebStateList* web_state_list = tabModel_.webStateList;
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::TestWebState> web_state = CreateTestWebState();
  web::WebState* web_state_ptr = web_state.get();
  web_state->SetCurrentURL(GURL("chrome://newtab"));
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  std::unique_ptr<web::TestWebState> web_state_2 = CreateTestWebState();
  web::WebState* web_state_ptr_2 = web_state_2.get();
  GURL url("http://test/2");
  web_state_2->SetCurrentURL(url);
  web_state_list->InsertWebState(1, std::move(web_state_2),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());

  web_state_list->ActivateWebStateAt(0);

  ASSERT_EQ(web_state_ptr, web_state_list->GetActiveWebState());

  service_->OpenUrl(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(web_state_ptr_2, web_state_list->GetActiveWebState());
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// Tests that trying to switch to a closed tab open from the NTP opens it in the
// NTP.
TEST_F(URLLoadingServiceTest, TestSwitchToClosedTab) {
  WebStateList* web_state_list = tabModel_.webStateList;
  ASSERT_EQ(0, web_state_list->count());

  std::unique_ptr<web::TestWebState> web_state = CreateTestWebState();
  web_state->SetCurrentURL(GURL("chrome://newtab"));
  web::WebState* web_state_ptr = web_state.get();
  web_state_list->InsertWebState(0, std::move(web_state),
                                 WebStateList::INSERT_FORCE_INDEX,
                                 WebStateOpener());
  web_state_list->ActivateWebStateAt(0);

  GURL url("http://test/2");

  service_->OpenUrl(
      UrlLoadParams::SwitchToTab(web::NavigationManager::WebLoadParams(url)));
  EXPECT_EQ(1, web_state_list->count());
  EXPECT_EQ(web_state_ptr, web_state_list->GetActiveWebState());
  EXPECT_EQ(0, app_service_->load_new_tab_call_count);
}

// TODO(crbug.com/907527): add tests for current tab and new tab.

}  // namespace
