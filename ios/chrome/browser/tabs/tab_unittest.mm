// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/ios/block_types.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_helper_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#import "ios/chrome/browser/ui/open_in_controller.h"
#import "ios/chrome/browser/ui/open_in_controller_testing.h"
#import "ios/chrome/browser/web/external_app_launcher.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/web_state/navigation_context_impl.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kAppSettingsUrl[] = "app-settings://";
const char kNewTabUrl[] = "chrome://newtab/";
const char kGoogleUserUrl[] = "http://google.com";
const char kGoogleRedirectUrl[] = "http://www.google.fr/";
static NSString* const kGoogleTitle = @"Google";
const char kOtherUserUrl[] = "http://other.com";
const char kOtherRedirectUrl[] = "http://other.fr/";
NSString* const kOtherTitle = @"Other";
const char kContentDispositionWithFilename[] =
    "attachment; filename=\"suggested_filename.pdf\"";
const char kContentDispositionWithoutFilename[] =
    "attachment; parameter=parameter_value";
const char kInvalidFilenameUrl[] = "http://www.hostname.com/";
const char kValidFilenameUrl[] = "http://www.hostname.com/filename.pdf";
}  // namespace

@interface ArrayTabModel : TabModel {
 @private
  NSMutableArray* _tabsForTesting;
}
@end

@implementation ArrayTabModel
- (instancetype)init {
  if ((self = [super init]))
    _tabsForTesting = [[NSMutableArray alloc] initWithCapacity:1];
  return self;
}

- (void)addTabForTesting:(Tab*)tab {
  [_tabsForTesting addObject:tab];
}

- (NSUInteger)indexOfTab:(Tab*)tab {
  return [_tabsForTesting indexOfObject:tab];
}

- (NSUInteger)count {
  return [_tabsForTesting count];
}

- (void)closeTabAtIndex:(NSUInteger)index {
  [_tabsForTesting removeObjectAtIndex:index];
}
@end

@interface ExternalAppLauncherMock : OCMockComplexTypeHelper
@end

@implementation ExternalAppLauncherMock
typedef BOOL (^openURLBlockType)(const GURL&, BOOL);

- (BOOL)openURL:(const GURL&)url linkClicked:(BOOL)linkClicked {
  return static_cast<openURLBlockType>([self blockForSelector:_cmd])(
      url, linkClicked);
}
@end

namespace {

// Observer of a QueryHistory request.
class HistoryQueryResultsObserver
    : public base::RefCountedThreadSafe<HistoryQueryResultsObserver> {
 public:
  HistoryQueryResultsObserver(base::RunLoop* run_loop) : run_loop_(run_loop) {}

  // Stores |results| and stops the current message loop.
  void ProcessResults(history::QueryResults* results) {
    results_.Swap(results);
    run_loop_->QuitWhenIdle();
  }
  history::QueryResults* results() { return &results_; }

 protected:
  friend base::RefCountedThreadSafe<HistoryQueryResultsObserver>;
  virtual ~HistoryQueryResultsObserver();

 private:
  history::QueryResults results_;
  base::RunLoop* run_loop_;
};

HistoryQueryResultsObserver::~HistoryQueryResultsObserver() {}

// TODO(crbug.com/620465): can a TestWebState be used instead of a WebStateImpl
// for those tests? This will require changing Tab to use a WebState instead of
// a WebStateImpl first though.
class TabTest : public BlockCleanupTest {
 public:
  TabTest()
      : thread_bundle_(web::TestWebThreadBundle::REAL_FILE_THREAD),
        scoped_browser_state_manager_(
            base::MakeUnique<TestChromeBrowserStateManager>(base::FilePath())),
        web_state_impl_(nullptr) {}

  void SetUp() override {
    BlockCleanupTest::SetUp();

    [[ChromeAppConstants sharedInstance]
        setCallbackSchemeForTesting:@"chromium"];

    // Set up the testing profiles.
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    chrome_browser_state_->CreateBookmarkModel(false);
    bookmarks::test::WaitForBookmarkModelToLoad(
        ios::BookmarkModelFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    ASSERT_TRUE(chrome_browser_state_->CreateHistoryService(true));

    ios::ChromeBrowserState* browser_state = chrome_browser_state_.get();
    if (UseOffTheRecordBrowserState())
      browser_state = browser_state->GetOffTheRecordChromeBrowserState();

    mock_web_controller_ =
        [OCMockObject niceMockForClass:[CRWWebController class]];
    web::WebState::CreateParams create_params(browser_state);
    web_state_impl_ = base::MakeUnique<web::WebStateImpl>(create_params);
    web_state_impl_->SetWebController(mock_web_controller_);
    web_state_impl_->GetNavigationManagerImpl().InitializeSession();
    web::WebStateImpl* web_state_impl = web_state_impl_.get();
    [[[static_cast<OCMockObject*>(mock_web_controller_) stub]
        andReturnValue:OCMOCK_VALUE(web_state_impl)] webStateImpl];
    web_controller_view_ = [[UIView alloc] init];
    [[[static_cast<OCMockObject*>(mock_web_controller_) stub]
        andReturn:web_controller_view_] view];
    LegacyTabHelper::CreateForWebState(web_state_impl_.get());
    tab_ = LegacyTabHelper::GetTabForWebState(web_state_impl_.get());
    web::NavigationManager::WebLoadParams load_params(
        GURL("chrome://version/"));
    [tab_ navigationManager]->LoadURLWithParams(load_params);

    // There should be no entries in the history at this point.
    history::QueryResults results;
    QueryAllHistory(&results);
    EXPECT_EQ(0UL, results.size());
    mock_external_app_launcher_ = [[ExternalAppLauncherMock alloc]
        initWithRepresentedObject:
            [OCMockObject mockForClass:[ExternalAppLauncher class]]];
    [tab_ replaceExternalAppLauncher:mock_external_app_launcher_];
  }

  void TearDown() override {
    // Ensure that the Tab is destroyed before the autorelease pool is cleared.
    web_state_impl_.reset();
    BlockCleanupTest::TearDown();
  }

  void BrowseTo(const GURL& userUrl, const GURL& redirectUrl, NSString* title) {
    DCHECK_EQ(tab_.webState, web_state_impl_.get());

    [tab_ webWillAddPendingURL:userUrl transition:ui::PAGE_TRANSITION_TYPED];
    std::unique_ptr<web::NavigationContext> context1 =
        web::NavigationContextImpl::CreateNavigationContext(
            web_state_impl_.get(), userUrl,
            ui::PageTransition::PAGE_TRANSITION_TYPED);
    web_state_impl_->OnNavigationStarted(context1.get());
    [tab_ webWillAddPendingURL:redirectUrl
                    transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT];

    web::Referrer empty_referrer;
    [tab_ navigationManagerImpl]->AddPendingItem(
        redirectUrl, empty_referrer, ui::PAGE_TRANSITION_CLIENT_REDIRECT,
        web::NavigationInitiationType::RENDERER_INITIATED,
        web::NavigationManager::UserAgentOverrideOption::INHERIT);

    std::unique_ptr<web::NavigationContext> context2 =
        web::NavigationContextImpl::CreateNavigationContext(
            web_state_impl_.get(), redirectUrl,
            ui::PageTransition::PAGE_TRANSITION_TYPED);
    web_state_impl_->OnNavigationStarted(context2.get());
    [tab_ navigationManagerImpl]->CommitPendingItem();
    web_state_impl_->UpdateHttpResponseHeaders(redirectUrl);
    web_state_impl_->OnNavigationFinished(context2.get());

    base::string16 new_title = base::SysNSStringToUTF16(title);
    [tab_ navigationManager]->GetLastCommittedItem()->SetTitle(new_title);

    web_state_impl_->OnTitleChanged();
    web_state_impl_->SetIsLoading(false);
    web_state_impl_->OnPageLoaded(redirectUrl, true);
  }

  void BrowseToNewTab() {
    DCHECK_EQ(tab_.webState, web_state_impl_.get());
    const GURL url(kNewTabUrl);
    // TODO(crbug.com/661992): This will not work with a mock CRWWebController.
    // The only test that uses it is currently disabled.
    web::NavigationManager::WebLoadParams params(url);
    params.transition_type = ui::PAGE_TRANSITION_TYPED;
    [tab_ navigationManager]->LoadURLWithParams(params);
    web_state_impl_->SetIsLoading(true);
    web_state_impl_->SetIsLoading(false);
    web_state_impl_->OnPageLoaded(url, true);
    web_state_impl_->OnTitleChanged();
  }

  void QueryAllHistory(history::QueryResults* results) {
    base::CancelableTaskTracker tracker;
    base::RunLoop run_loop;
    scoped_refptr<HistoryQueryResultsObserver> observer(
        new HistoryQueryResultsObserver(&run_loop));
    history::HistoryService* history_service =
        ios::HistoryServiceFactory::GetForBrowserState(
            chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    history_service->QueryHistory(
        base::string16(), history::QueryOptions(),
        base::Bind(&HistoryQueryResultsObserver::ProcessResults, observer),
        &tracker);
    run_loop.Run();
    results->Swap(observer->results());
  }

  void CheckHistoryResult(const history::URLResult& historyResult,
                          const GURL& expectedUrl,
                          NSString* expectedTitle) {
    EXPECT_EQ(expectedUrl, historyResult.url());
    EXPECT_EQ(base::SysNSStringToUTF16(expectedTitle), historyResult.title());
  }

  void CheckCurrentItem(const GURL& expectedUrl, NSString* expectedTitle) {
    web::NavigationItem* item = [tab_ navigationManager]->GetVisibleItem();
    EXPECT_EQ(expectedUrl, item->GetURL());
    EXPECT_EQ(base::SysNSStringToUTF16(expectedTitle), item->GetTitle());
  }

  void CheckCurrentItem(const history::URLResult& historyResult) {
    web::NavigationItem* item = [tab_ navigationManager]->GetVisibleItem();
    CheckHistoryResult(historyResult, item->GetURL(),
                       base::SysUTF16ToNSString(item->GetTitle()));
  }

#ifndef NDEBUG
  // Method useful when debugging.
  void LogHistoryQueryResult(const history::QueryResults* results) {
    for (const auto& result : *results) {
      NSLog(@"title = %@; url = %@", base::SysUTF16ToNSString(result.title()),
            base::SysUTF8ToNSString(result.url().spec()));
    }
  }
#endif

  virtual bool UseOffTheRecordBrowserState() const { return false; }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<web::WebStateImpl> web_state_impl_;
  __weak CRWWebController* mock_web_controller_;
  UIView* web_controller_view_;
  ArrayTabModel* tabModel_;
  id mock_external_app_launcher_;
  __weak Tab* tab_;
};

TEST_F(TabTest, AddToHistoryWithRedirect) {
  BrowseTo(GURL(kGoogleUserUrl), GURL(kGoogleRedirectUrl), kGoogleTitle);
  history::QueryResults results;
  QueryAllHistory(&results);
  EXPECT_EQ(1U, results.size());
  CheckHistoryResult(results[0], GURL(kGoogleRedirectUrl), kGoogleTitle);
  CheckCurrentItem(results[0]);
}

TEST_F(TabTest, FailToOpenAppSettings) {
  GURL app_settings_url = GURL(kAppSettingsUrl);
  BOOL will_open_app_settings =
      [tab_ openExternalURL:app_settings_url sourceURL:GURL() linkClicked:YES];
  EXPECT_FALSE(will_open_app_settings);
}

// TODO(crbug.com/378098): Disabled because forward/back is now implemented in
// CRWWebController, so this test cannot function with a mock CRWWebController.
// Rewrite and re-enable this test when it becomes a CRWWebController or
// NavigationManager test.
TEST_F(TabTest, DISABLED_BackAndForward) {
  BrowseTo(GURL(kGoogleUserUrl), GURL(kGoogleRedirectUrl), kGoogleTitle);
  BrowseTo(GURL(kOtherUserUrl), GURL(kOtherRedirectUrl), kOtherTitle);

  history::QueryResults results;
  QueryAllHistory(&results);
  EXPECT_EQ(2U, results.size());
  CheckHistoryResult(results[0], GURL(kOtherRedirectUrl), kOtherTitle);
  CheckHistoryResult(results[1], GURL(kGoogleRedirectUrl), kGoogleTitle);
  CheckCurrentItem(results[0]);
}

// TODO(crbug.com/378098): Disabled because navigation is no longer
// possible with a mock
// CRWWebController. Rewrite and re-enable this test when it becomes a
// CRWWebController test.
TEST_F(TabTest, DISABLED_NewTabInMiddleOfNavigation) {
  BrowseTo(GURL(kGoogleUserUrl), GURL(kGoogleRedirectUrl), kGoogleTitle);
  BrowseToNewTab();
  BrowseTo(GURL(kOtherUserUrl), GURL(kOtherRedirectUrl), kOtherTitle);

  // NOTE: NTP is not stored in the history.
  history::QueryResults results;
  QueryAllHistory(&results);
  EXPECT_EQ(2U, results.size());
  CheckHistoryResult(results[0], GURL(kOtherRedirectUrl), kOtherTitle);
  CheckHistoryResult(results[1], GURL(kGoogleRedirectUrl), kGoogleTitle);
}

TEST_F(TabTest, GetSuggestedFilenameFromContentDisposition) {
  // If possible, the filename should be generated from the content-disposition
  // header.
  GURL url(kValidFilenameUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("HTTP 1.1 200 OK");
  headers->AddHeader(base::StringPrintf("Content-Type: application/pdf"));
  headers->AddHeader(base::StringPrintf("Content-Disposition: %s",
                                        kContentDispositionWithFilename));
  web_state_impl_->OnHttpResponseHeadersReceived(headers.get(), url);
  BrowseTo(url, url, [NSString string]);
  EXPECT_NSEQ(@"suggested_filename.pdf",
              [[tab_ openInController] suggestedFilename]);
}

TEST_F(TabTest, GetSuggestedFilenameFromURL) {
  // If the content-disposition header does not specify a filename, this should
  // be extracted from the last component of the url.
  GURL url(kValidFilenameUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("HTTP 1.1 200 OK");
  headers->AddHeader(base::StringPrintf("Content-Type: application/pdf"));
  headers->AddHeader(base::StringPrintf("Content-Disposition: %s",
                                        kContentDispositionWithoutFilename));
  web_state_impl_->OnHttpResponseHeadersReceived(headers.get(), url);
  BrowseTo(url, url, [NSString string]);
  EXPECT_NSEQ(@"filename.pdf", [[tab_ openInController] suggestedFilename]);
}

TEST_F(TabTest, GetSuggestedFilenameFromDefaultName) {
  // If the filename cannot be extracted from the content disposition or from
  // the url, the default filename "Document.pdf" should be used.
  GURL url(kInvalidFilenameUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("HTTP 1.1 200 OK");
  headers->AddHeader(base::StringPrintf("Content-Type: application/pdf"));
  web_state_impl_->OnHttpResponseHeadersReceived(headers.get(), url);
  BrowseTo(url, url, [NSString string]);
  EXPECT_NSEQ(@"Document.pdf", [[tab_ openInController] suggestedFilename]);
}

}  // namespace
