// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <objc/runtime.h>

#include "base/files/file_path.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/sessions/session_window.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/crw_session_storage.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/serializable_user_data_manager.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

using web::WebStateImpl;

// For some of the unit tests, we need to make sure the session is saved
// immediately so we can read it back in to verify various attributes. This
// is not a situation we normally expect to be in because we never
// want the session being saved on the main thread in the production app.
// We could expose this as part of the service's public API, but again that
// might encourage use where we don't want it. As a result, just use the
// known private-for-testing method directly.
@interface SessionServiceIOS (Testing)
- (void)performSaveWindow:(SessionWindowIOS*)window
              toDirectory:(NSString*)directory;
@end

@interface TabModel (VisibleForTesting)
- (SessionWindowIOS*)windowForSavingSession;
@end

// Defines a TabModelObserver for use in unittests.  This class can be used to
// test if an observer method was called or not.
@interface TabModelObserverPong : NSObject<TabModelObserver> {
  // TODO(crbug.com/661989): Add tests for the other observer methods.
  BOOL tabMovedWasCalled_;
}
@property(nonatomic, assign) BOOL tabMovedWasCalled;
@end

@implementation TabModelObserverPong
@synthesize tabMovedWasCalled = tabMovedWasCalled_;

- (void)tabModel:(TabModel*)model
      didMoveTab:(Tab*)tab
       fromIndex:(NSUInteger)fromIndex
         toIndex:(NSUInteger)toIndex {
  tabMovedWasCalled_ = YES;
}

@end

namespace {

const GURL kURL("https://www.some.url.com");
const web::Referrer kEmptyReferrer;
const web::Referrer kReferrer(GURL("https//www.some.referer.com"),
                              web::ReferrerPolicyDefault);
const web::Referrer kReferrer2(GURL("https//www.some.referer2.com"),
                               web::ReferrerPolicyDefault);

class TabModelTest : public PlatformTest {
 public:
  TabModelTest()
      : scoped_browser_state_manager_(
            base::MakeUnique<TestChromeBrowserStateManager>(base::FilePath())),
        web_client_(base::MakeUnique<ChromeWebClient>()) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);

    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    session_window_.reset([[SessionWindowIOS alloc] init]);
    // Create tab model with just a dummy session service so the async state
    // saving doesn't trigger unless actually wanted.
    base::scoped_nsobject<TestSessionService> test_service(
        [[TestSessionService alloc] init]);
    tab_model_.reset([[TabModel alloc]
        initWithSessionWindow:session_window_.get()
               sessionService:test_service
                 browserState:chrome_browser_state_.get()]);
    [tab_model_ setWebUsageEnabled:NO];
    [tab_model_ setPrimary:YES];
    tab_model_observer_.reset([[TabModelObserverPong alloc] init]);
    [tab_model_ addObserver:tab_model_observer_];
  }

  ~TabModelTest() override {
    [tab_model_ removeObserver:tab_model_observer_];
    [tab_model_ browserStateDestroyed];
  }

 protected:
  std::unique_ptr<WebStateImpl> CreateWebState(NSString* opener,
                                               NSInteger index) {
    auto webState = base::MakeUnique<WebStateImpl>(chrome_browser_state_.get());
    webState->GetNavigationManagerImpl().InitializeSession(NO);
    if ([opener length] != 0) {
      // Duplicate code from Tab initializer. Will be removed once the code
      // is rewritten to remove the use of internal ios/web/ API (see issue
      // http://crbug.com/620465 for progress).
      web::SerializableUserDataManager* userDataManager =
          web::SerializableUserDataManager::FromWebState(webState.get());
      userDataManager->AddSerializableData(opener, @"OpenerID");
      userDataManager->AddSerializableData(@(index), @"OpenerNavigationIndex");
    }
    return webState;
  }

  std::unique_ptr<WebStateImpl> CreateWebState() {
    return CreateWebState(@"", -1);
  }

  std::unique_ptr<WebStateImpl> CreateChildWebState(Tab* parent) {
    return CreateWebState(parent.tabId, -1);
  }

  void RestoreSession(SessionWindowIOS* window) {
    [tab_model_ restoreSessionWindow:window];
  }

  // Creates a session window with |entries| entries and a |selectedIndex| of 1.
  SessionWindowIOS* CreateSessionWindow(int entries) {
    SessionWindowIOS* window = [[SessionWindowIOS alloc] init];
    for (int i = 0; i < entries; i++) {
      CRWSessionStorage* session_storage =
          [[[CRWSessionStorage alloc] init] autorelease];
      [window addSerializedSessionStorage:session_storage];
    }
    if (entries)
      [window setSelectedIndex:1];
    return window;
  }

  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  web::ScopedTestingWebClient web_client_;
  base::scoped_nsobject<SessionWindowIOS> session_window_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::mac::ScopedNSAutoreleasePool pool_;
  base::scoped_nsobject<TabModel> tab_model_;
  base::scoped_nsobject<TabModelObserverPong> tab_model_observer_;
};

TEST_F(TabModelTest, IsEmpty) {
  EXPECT_EQ([tab_model_ count], 0U);
  EXPECT_TRUE([tab_model_ isEmpty]);
  [tab_model_ insertTabWithURL:kURL
                      referrer:kReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:0
                  inBackground:YES];
  ASSERT_EQ(1U, [tab_model_ count]);
  EXPECT_FALSE([tab_model_ isEmpty]);
}

TEST_F(TabModelTest, InsertUrlSingle) {
  Tab* tab = [tab_model_ insertTabWithURL:kURL
                                 referrer:kReferrer
                               transition:ui::PAGE_TRANSITION_TYPED
                                   opener:nil
                              openedByDOM:NO
                                  atIndex:0
                             inBackground:YES];
  ASSERT_EQ(1U, [tab_model_ count]);
  EXPECT_NSEQ(tab, [tab_model_ tabAtIndex:0]);
}

TEST_F(TabModelTest, InsertUrlMultiple) {
  Tab* tab0 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:0
                              inBackground:YES];
  Tab* tab1 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:0
                              inBackground:YES];
  Tab* tab2 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:1
                              inBackground:YES];

  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:2]);
}

TEST_F(TabModelTest, AppendUrlSingle) {
  Tab* tab = [tab_model_ insertTabWithURL:kURL
                                 referrer:kReferrer
                               transition:ui::PAGE_TRANSITION_TYPED
                                   opener:nil
                              openedByDOM:NO
                                  atIndex:[tab_model_ count]
                             inBackground:YES];
  ASSERT_EQ(1U, [tab_model_ count]);
  EXPECT_NSEQ(tab, [tab_model_ tabAtIndex:0]);
}

TEST_F(TabModelTest, AppendUrlMultiple) {
  Tab* tab0 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];
  Tab* tab1 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];
  Tab* tab2 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];

  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:2]);
}

TEST_F(TabModelTest, CloseTabAtIndexBeginning) {
  [tab_model_ insertTabWithURL:kURL
                      referrer:kReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  Tab* tab1 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];
  Tab* tab2 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];

  [tab_model_ closeTabAtIndex:0];

  ASSERT_EQ(2U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
}

TEST_F(TabModelTest, CloseTabAtIndexMiddle) {
  Tab* tab0 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  Tab* tab2 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];

  [tab_model_ closeTabAtIndex:1];

  ASSERT_EQ(2U, [tab_model_ count]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
}

TEST_F(TabModelTest, CloseTabAtIndexLast) {
  Tab* tab0 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];
  Tab* tab1 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  [tab_model_ closeTabAtIndex:2];

  ASSERT_EQ(2U, [tab_model_ count]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:1]);
}

TEST_F(TabModelTest, CloseTabAtIndexOnlyOne) {
  [tab_model_ insertTabWithURL:kURL
                      referrer:kReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  [tab_model_ closeTabAtIndex:0];

  EXPECT_EQ(0U, [tab_model_ count]);
}

TEST_F(TabModelTest, RestoreSessionOnNTPTest) {
  Tab* tab = [tab_model_ insertTabWithURL:GURL(kChromeUINewTabURL)
                                 referrer:kEmptyReferrer
                               transition:ui::PAGE_TRANSITION_TYPED
                                   opener:nil
                              openedByDOM:NO
                                  atIndex:0
                             inBackground:YES];
  base::scoped_nsobject<SessionWindowIOS> window(CreateSessionWindow(3));

  RestoreSession(window.get());
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ([tab_model_ tabAtIndex:1], [tab_model_ currentTab]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:0]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:1]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:2]);
}

TEST_F(TabModelTest, RestoreSessionOn2NtpTest) {
  Tab* tab0 = [tab_model_ insertTabWithURL:GURL(kChromeUINewTabURL)
                                  referrer:kEmptyReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:0
                              inBackground:YES];
  Tab* tab1 = [tab_model_ insertTabWithURL:GURL(kChromeUINewTabURL)
                                  referrer:kEmptyReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:1
                              inBackground:YES];
  base::scoped_nsobject<SessionWindowIOS> window(CreateSessionWindow(3));

  RestoreSession(window.get());
  ASSERT_EQ(5U, [tab_model_ count]);
  EXPECT_NSEQ([tab_model_ tabAtIndex:3], [tab_model_ currentTab]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:1]);
  EXPECT_NSNE(tab0, [tab_model_ tabAtIndex:2]);
  EXPECT_NSNE(tab0, [tab_model_ tabAtIndex:3]);
  EXPECT_NSNE(tab0, [tab_model_ tabAtIndex:4]);
  EXPECT_NSNE(tab1, [tab_model_ tabAtIndex:2]);
  EXPECT_NSNE(tab1, [tab_model_ tabAtIndex:3]);
  EXPECT_NSNE(tab1, [tab_model_ tabAtIndex:4]);
}

TEST_F(TabModelTest, RestoreSessionOnAnyTest) {
  Tab* tab = [tab_model_ insertTabWithURL:kURL
                                 referrer:kEmptyReferrer
                               transition:ui::PAGE_TRANSITION_TYPED
                                   opener:nil
                              openedByDOM:NO
                                  atIndex:0
                             inBackground:YES];
  base::scoped_nsobject<SessionWindowIOS> window(CreateSessionWindow(3));

  RestoreSession(window.get());
  ASSERT_EQ(4U, [tab_model_ count]);
  EXPECT_NSEQ([tab_model_ tabAtIndex:2], [tab_model_ currentTab]);
  EXPECT_NSEQ(tab, [tab_model_ tabAtIndex:0]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:1]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:2]);
  EXPECT_NSNE(tab, [tab_model_ tabAtIndex:3]);
}

TEST_F(TabModelTest, CloseAllTabs) {
  [tab_model_ insertTabWithURL:kURL
                      referrer:kReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:GURL("https://www.some.url2.com")
                      referrer:kReferrer2
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  [tab_model_ closeAllTabs];

  EXPECT_EQ(0U, [tab_model_ count]);
}

TEST_F(TabModelTest, CloseAllTabsWithNoTabs) {
  [tab_model_ closeAllTabs];

  EXPECT_EQ(0U, [tab_model_ count]);
}

TEST_F(TabModelTest, InsertWithSessionController) {
  EXPECT_EQ([tab_model_ count], 0U);
  EXPECT_TRUE([tab_model_ isEmpty]);

  Tab* new_tab = [tab_model_ insertTabWithWebState:CreateWebState(@"opener", -1)
                                           atIndex:0];
  EXPECT_EQ([tab_model_ count], 1U);
  [tab_model_ setCurrentTab:new_tab];
  Tab* current_tab = [tab_model_ currentTab];
  EXPECT_TRUE(current_tab);
}

TEST_F(TabModelTest, OpenerOfTab) {
  // Start off with a couple tabs.
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  // Create parent tab.
  Tab* parent_tab = [tab_model_ insertTabWithWebState:CreateWebState()
                                              atIndex:[tab_model_ count]];
  // Create child tab.
  Tab* child_tab =
      [tab_model_ insertTabWithWebState:CreateChildWebState(parent_tab)
                                atIndex:[tab_model_ count]];
  // Create another unrelated tab.
  Tab* another_tab = [tab_model_ insertTabWithWebState:CreateWebState()
                                               atIndex:[tab_model_ count]];

  // Create another child of the first tab.
  Tab* child_tab2 =
      [tab_model_ insertTabWithWebState:CreateChildWebState(parent_tab)
                                atIndex:[tab_model_ count]];

  EXPECT_FALSE([tab_model_ openerOfTab:parent_tab]);
  EXPECT_FALSE([tab_model_ openerOfTab:another_tab]);
  EXPECT_EQ(parent_tab, [tab_model_ openerOfTab:child_tab]);
  EXPECT_EQ(parent_tab, [tab_model_ openerOfTab:child_tab2]);
}

TEST_F(TabModelTest, OpenerOfTabEmptyModel) {
  EXPECT_FALSE([tab_model_ openerOfTab:nil]);
}

TEST_F(TabModelTest, OpenersEmptyModel) {
  // Empty model.
  EXPECT_TRUE([tab_model_ isEmpty]);
  EXPECT_FALSE([tab_model_ nextTabWithOpener:nil afterTab:nil]);
  EXPECT_FALSE([tab_model_ lastTabWithOpener:nil]);
}

TEST_F(TabModelTest, OpenersNothingOpenedGeneral) {
  // Start with a few tabs.
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  Tab* tab = [tab_model_ insertTabWithWebState:CreateWebState()
                                       atIndex:[tab_model_ count]];

  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  // All should fail since this hasn't opened anything else.
  EXPECT_FALSE([tab_model_ nextTabWithOpener:tab afterTab:nil]);
  EXPECT_FALSE([tab_model_ lastTabWithOpener:tab]);

  // Add more items to the tab, expect the same results.
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  EXPECT_FALSE([tab_model_ nextTabWithOpener:tab afterTab:nil]);
  EXPECT_FALSE([tab_model_ lastTabWithOpener:tab]);
}

TEST_F(TabModelTest, OpenersNothingOpenedFirst) {
  // Our tab is first.
  Tab* tab = [tab_model_ insertTabWithWebState:CreateWebState()
                                       atIndex:[tab_model_ count]];

  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  // All should fail since this hasn't opened anything else.
  EXPECT_FALSE([tab_model_ nextTabWithOpener:tab afterTab:nil]);
  EXPECT_FALSE([tab_model_ lastTabWithOpener:tab]);
}

TEST_F(TabModelTest, OpenersNothingOpenedLast) {
  // Our tab is last.
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  Tab* tab = [tab_model_ insertTabWithWebState:CreateWebState()
                                       atIndex:[tab_model_ count]];

  // All should fail since this hasn't opened anything else.
  EXPECT_FALSE([tab_model_ nextTabWithOpener:tab afterTab:nil]);
  EXPECT_FALSE([tab_model_ lastTabWithOpener:tab]);
}

TEST_F(TabModelTest, OpenersChildTabBeforeOpener) {
  Tab* parent_tab = [tab_model_ insertTabWithWebState:CreateWebState()
                                              atIndex:[tab_model_ count]];
  // Insert child at start
  [tab_model_ insertTabWithWebState:CreateChildWebState(parent_tab) atIndex:0];

  // Insert a few more between them.
  [tab_model_ insertTabWithWebState:CreateWebState() atIndex:1];
  [tab_model_ insertTabWithWebState:CreateWebState() atIndex:1];

  EXPECT_FALSE([tab_model_ nextTabWithOpener:parent_tab afterTab:nil]);
  EXPECT_FALSE([tab_model_ lastTabWithOpener:parent_tab]);
}

TEST_F(TabModelTest, OpenersChildTabAfterOpener) {
  Tab* parent_tab = [tab_model_ insertTabWithWebState:CreateWebState()
                                              atIndex:[tab_model_ count]];

  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  // Insert two children at end.
  Tab* child_tab1 =
      [tab_model_ insertTabWithWebState:CreateChildWebState(parent_tab)
                                atIndex:[tab_model_ count]];
  Tab* child_tab2 =
      [tab_model_ insertTabWithWebState:CreateChildWebState(parent_tab)
                                atIndex:[tab_model_ count]];

  EXPECT_EQ([tab_model_ nextTabWithOpener:parent_tab afterTab:nil], child_tab1);
  EXPECT_EQ([tab_model_ nextTabWithOpener:parent_tab afterTab:child_tab1],
            child_tab2);
  EXPECT_EQ([tab_model_ lastTabWithOpener:parent_tab], child_tab2);
}

TEST_F(TabModelTest, AddWithOrderController) {
  // Create a few tabs with the controller at the front.
  Tab* parent = [tab_model_ insertTabWithURL:kURL
                                    referrer:kEmptyReferrer
                                  transition:ui::PAGE_TRANSITION_TYPED
                                      opener:nil
                                 openedByDOM:NO
                                     atIndex:[tab_model_ count]
                                inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  // Add a new tab, it should be added behind the parent.
  Tab* child =
      [tab_model_ insertTabWithURL:kURL
                          referrer:kEmptyReferrer
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:parent
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:parent], 0U);
  EXPECT_EQ([tab_model_ indexOfTab:child], 1U);

  // Add another new tab without a parent, should go at the end.
  Tab* tab =
      [tab_model_ insertTabWithURL:kURL
                          referrer:kEmptyReferrer
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:nil
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:tab], [tab_model_ count] - 1);

  // Same for a tab that's not opened via a LINK transition.
  Tab* tab2 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kEmptyReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:tab2], [tab_model_ count] - 1);

  // Add a tab in the background. It should appear behind the opening tab.
  Tab* tab3 =
      [tab_model_ insertTabWithURL:kURL
                          referrer:kEmptyReferrer
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:tab
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:YES];
  EXPECT_EQ([tab_model_ indexOfTab:tab3], [tab_model_ indexOfTab:tab] + 1);

  // Add another background tab behind the one we just opened.
  Tab* tab4 =
      [tab_model_ insertTabWithURL:kURL
                          referrer:kEmptyReferrer
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:tab3
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:YES];
  EXPECT_EQ([tab_model_ indexOfTab:tab4], [tab_model_ indexOfTab:tab3] + 1);
}

TEST_F(TabModelTest, AddWithOrderControllerAndGrouping) {
  // Create a few tabs with the controller at the front.
  Tab* parent = [tab_model_ insertTabWithURL:kURL
                                    referrer:kEmptyReferrer
                                  transition:ui::PAGE_TRANSITION_TYPED
                                      opener:nil
                                 openedByDOM:NO
                                     atIndex:[tab_model_ count]
                                inBackground:YES];
  // Force the history to update, as it is used to determine grouping.
  ASSERT_TRUE([parent navigationManagerImpl]);
  [[parent navigationManagerImpl]->GetSessionController() commitPendingItem];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  ASSERT_TRUE(chrome_browser_state_->CreateHistoryService(true));

  // Add a new tab, it should be added behind the parent.
  Tab* child1 =
      [tab_model_ insertTabWithURL:kURL
                          referrer:kEmptyReferrer
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:parent
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:parent], 0U);
  EXPECT_EQ([tab_model_ indexOfTab:child1], 1U);

  // Add a second child tab in the background. It should be added behind the
  // first child.
  Tab* child2 =
      [tab_model_ insertTabWithURL:kURL
                          referrer:kEmptyReferrer
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:parent
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:YES];
  EXPECT_EQ([tab_model_ indexOfTab:child2], 2U);

  // Navigate the parent tab to a new URL.  It should not change any ordering.
  web::NavigationManager::WebLoadParams parent_params(
      GURL("http://www.espn.com"));
  parent_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  [[parent webController] loadWithParams:parent_params];
  ASSERT_TRUE([parent navigationManagerImpl]);
  [[parent navigationManagerImpl]->GetSessionController() commitPendingItem];
  EXPECT_EQ([tab_model_ indexOfTab:parent], 0U);

  // Add a new tab. It should be added behind the parent. It should not be added
  // after the previous two children.
  Tab* child3 =
      [tab_model_ insertTabWithURL:kURL
                          referrer:kEmptyReferrer
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:parent
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:child3], 1U);

  // Add a fourt child tab in the background. It should be added behind the
  // third child.
  Tab* child4 =
      [tab_model_ insertTabWithURL:kURL
                          referrer:kEmptyReferrer
                        transition:ui::PAGE_TRANSITION_LINK
                            opener:parent
                       openedByDOM:NO
                           atIndex:TabModelConstants::kTabPositionAutomatically
                      inBackground:YES];
  EXPECT_EQ([tab_model_ indexOfTab:child4], 2U);

  // The first two children should have been moved to the right.
  EXPECT_EQ([tab_model_ indexOfTab:child1], 3U);
  EXPECT_EQ([tab_model_ indexOfTab:child2], 4U);

  // Now add a non-owned tab and make sure it is added at the end.
  Tab* nonChild = [tab_model_ insertTabWithURL:kURL
                                      referrer:kEmptyReferrer
                                    transition:ui::PAGE_TRANSITION_TYPED
                                        opener:nil
                                   openedByDOM:NO
                                       atIndex:[tab_model_ count]
                                  inBackground:YES];
  EXPECT_EQ([tab_model_ indexOfTab:nonChild], [tab_model_ count] - 1);
}

TEST_F(TabModelTest, AddWithLinkTransitionAndIndex) {
  // Create a few tabs with the controller at the front.
  Tab* parent = [tab_model_ insertTabWithURL:kURL
                                    referrer:kEmptyReferrer
                                  transition:ui::PAGE_TRANSITION_TYPED
                                      opener:nil
                                 openedByDOM:NO
                                     atIndex:[tab_model_ count]
                                inBackground:YES];
  // Force the history to update, as it is used to determine grouping.
  ASSERT_TRUE([parent navigationManagerImpl]);
  [[parent navigationManagerImpl]->GetSessionController() commitPendingItem];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];
  [tab_model_ insertTabWithURL:kURL
                      referrer:kEmptyReferrer
                    transition:ui::PAGE_TRANSITION_TYPED
                        opener:nil
                   openedByDOM:NO
                       atIndex:[tab_model_ count]
                  inBackground:YES];

  ASSERT_TRUE(chrome_browser_state_->CreateHistoryService(true));

  // Add a new tab, it should be added before the parent since the index
  // parameter has been specified with a valid value.
  Tab* child1 = [tab_model_ insertTabWithURL:kURL
                                    referrer:kEmptyReferrer
                                  transition:ui::PAGE_TRANSITION_LINK
                                      opener:parent
                                 openedByDOM:NO
                                     atIndex:0
                                inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:parent], 1U);
  EXPECT_EQ([tab_model_ indexOfTab:child1], 0U);

  // Add a new tab, it should be added at the beginning of the stack because
  // the index parameter has been specified with a valid value.
  Tab* child2 = [tab_model_ insertTabWithURL:kURL
                                    referrer:kEmptyReferrer
                                  transition:ui::PAGE_TRANSITION_LINK
                                      opener:parent
                                 openedByDOM:NO
                                     atIndex:0
                                inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:parent], 2U);
  EXPECT_EQ([tab_model_ indexOfTab:child1], 1U);
  EXPECT_EQ([tab_model_ indexOfTab:child2], 0U);

  // Add a new tab, it should be added at position 1 because the index parameter
  // has been specified with a valid value.
  Tab* child3 = [tab_model_ insertTabWithURL:kURL
                                    referrer:kEmptyReferrer
                                  transition:ui::PAGE_TRANSITION_LINK
                                      opener:parent
                                 openedByDOM:NO
                                     atIndex:1
                                inBackground:NO];
  EXPECT_EQ([tab_model_ indexOfTab:parent], 3U);
  EXPECT_EQ([tab_model_ indexOfTab:child1], 2U);
  EXPECT_EQ([tab_model_ indexOfTab:child3], 1U);
  EXPECT_EQ([tab_model_ indexOfTab:child2], 0U);
}

TEST_F(TabModelTest, MoveTabs) {
  Tab* tab0 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];
  Tab* tab1 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];
  Tab* tab2 = [tab_model_ insertTabWithURL:kURL
                                  referrer:kReferrer
                                transition:ui::PAGE_TRANSITION_TYPED
                                    opener:nil
                               openedByDOM:NO
                                   atIndex:[tab_model_ count]
                              inBackground:YES];

  // Basic sanity checks before moving on.
  ASSERT_EQ(3U, [tab_model_ count]);
  ASSERT_NSEQ(tab0, [tab_model_ tabAtIndex:0]);
  ASSERT_NSEQ(tab1, [tab_model_ tabAtIndex:1]);
  ASSERT_NSEQ(tab2, [tab_model_ tabAtIndex:2]);

  // Move a tab from index 1 to index 0 (move tab left by one).
  [tab_model_observer_ setTabMovedWasCalled:NO];
  [tab_model_ moveTab:[tab_model_ tabAtIndex:1] toIndex:0];
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:2]);
  EXPECT_TRUE([tab_model_observer_ tabMovedWasCalled]);

  // Move a tab from index 1 to index 2 (move tab right by one).
  [tab_model_observer_ setTabMovedWasCalled:NO];
  [tab_model_ moveTab:[tab_model_ tabAtIndex:1] toIndex:2];
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:2]);
  EXPECT_TRUE([tab_model_observer_ tabMovedWasCalled]);

  // Move a tab from index 0 to index 2 (move tab right by more than one).
  [tab_model_observer_ setTabMovedWasCalled:NO];
  [tab_model_ moveTab:[tab_model_ tabAtIndex:0] toIndex:2];
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:2]);
  EXPECT_TRUE([tab_model_observer_ tabMovedWasCalled]);

  // Move a tab from index 2 to index 0 (move tab left by more than one).
  [tab_model_observer_ setTabMovedWasCalled:NO];
  [tab_model_ moveTab:[tab_model_ tabAtIndex:2] toIndex:0];
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:2]);
  EXPECT_TRUE([tab_model_observer_ tabMovedWasCalled]);

  // Move a tab from index 2 to index 2 (move tab to the same index).
  [tab_model_observer_ setTabMovedWasCalled:NO];
  [tab_model_ moveTab:[tab_model_ tabAtIndex:2] toIndex:2];
  ASSERT_EQ(3U, [tab_model_ count]);
  EXPECT_NSEQ(tab1, [tab_model_ tabAtIndex:0]);
  EXPECT_NSEQ(tab2, [tab_model_ tabAtIndex:1]);
  EXPECT_NSEQ(tab0, [tab_model_ tabAtIndex:2]);
  EXPECT_FALSE([tab_model_observer_ tabMovedWasCalled]);
}

TEST_F(TabModelTest, SetParentModel) {
  // Create a tab without a parent model and make sure it doesn't crash.  Then
  // set its parent TabModel and make sure that works as well.
  base::scoped_nsobject<Tab> tab([[Tab alloc]
      initWithBrowserState:chrome_browser_state_.get()
                    opener:nil
               openedByDOM:NO
                     model:nil]);
  EXPECT_TRUE([tab parentTabModel] == nil);
  [tab_model_ insertTab:tab atIndex:0];
  [tab setParentTabModel:tab_model_.get()];
  EXPECT_FALSE([tab parentTabModel] == nil);
  [tab_model_ closeTabAtIndex:0];
}

TEST_F(TabModelTest, PersistSelectionChange) {
  TestChromeBrowserState::Builder test_cbs_builder;
  auto chrome_browser_state = test_cbs_builder.Build();

  // Tabs register some observers with the ChromeBrowserState in an ObserverList
  // that assert it is empty in its destructor. As Tab are Objective-C object,
  // it is necessary to use a local pool to ensure all autoreleased object that
  // may reference those Tabs are deallocated before the TestChromeBrowserState
  // is destroyed (this cannot use the TabModelTest ScopedNSAutoreleasePool as
  // it will be drained after the local variable chrome_browser_state).
  base::mac::ScopedNSAutoreleasePool pool;

  NSString* stashPath =
      base::SysUTF8ToNSString(chrome_browser_state->GetStatePath().value());

  base::scoped_nsobject<TabModel> model([[TabModel alloc]
      initWithSessionWindow:session_window_.get()
             sessionService:[SessionServiceIOS sharedService]
               browserState:chrome_browser_state.get()]);

  [model insertTabWithURL:kURL
                 referrer:kReferrer
               transition:ui::PAGE_TRANSITION_TYPED
                   opener:nil
              openedByDOM:NO
                  atIndex:[model count]
             inBackground:YES];
  [model insertTabWithURL:kURL
                 referrer:kReferrer
               transition:ui::PAGE_TRANSITION_TYPED
                   opener:[model tabAtIndex:0]
              openedByDOM:NO
                  atIndex:[model count]
             inBackground:YES];
  [model insertTabWithURL:kURL
                 referrer:kReferrer
               transition:ui::PAGE_TRANSITION_TYPED
                   opener:[model tabAtIndex:1]
              openedByDOM:NO
                  atIndex:0
             inBackground:YES];

  ASSERT_EQ(3U, [model count]);
  [model setCurrentTab:[model tabAtIndex:1]];

  EXPECT_EQ(nil, [model openerOfTab:[model tabAtIndex:1]]);
  EXPECT_EQ([model tabAtIndex:1], [model openerOfTab:[model tabAtIndex:2]]);
  EXPECT_EQ([model tabAtIndex:2], [model openerOfTab:[model tabAtIndex:0]]);

  // Force state to flush to disk on the main thread so it can be immediately
  // tested below.
  SessionWindowIOS* window = [model windowForSavingSession];
  [[SessionServiceIOS sharedService] performSaveWindow:window
                                           toDirectory:stashPath];
  [model browserStateDestroyed];
  model.reset();

  // Restoring TabModel session sends asynchronous tasks to IO thread, wait
  // for them to complete after destroying the TabModel.
  base::RunLoop().RunUntilIdle();

  SessionWindowIOS* sessionWindow = [[SessionServiceIOS sharedService]
      loadWindowForBrowserState:chrome_browser_state.get()];

  // Create tab model from saved session.
  base::scoped_nsobject<TestSessionService> test_service(
      [[TestSessionService alloc] init]);

  model.reset([[TabModel alloc]
      initWithSessionWindow:sessionWindow
             sessionService:test_service
               browserState:chrome_browser_state.get()]);
  ASSERT_EQ(3u, [model count]);

  EXPECT_EQ([model tabAtIndex:1], [model currentTab]);
  EXPECT_EQ(nil, [model openerOfTab:[model tabAtIndex:1]]);
  EXPECT_EQ([model tabAtIndex:1], [model openerOfTab:[model tabAtIndex:2]]);
  EXPECT_EQ([model tabAtIndex:2], [model openerOfTab:[model tabAtIndex:0]]);

  [model browserStateDestroyed];
  model.reset();

  // Restoring TabModel session sends asynchronous tasks to IO thread, wait
  // for them to complete after destroying the TabModel.
  base::RunLoop().RunUntilIdle();

  // Clean up.
  EXPECT_TRUE([[NSFileManager defaultManager] removeItemAtPath:stashPath
                                                         error:nullptr]);
}

}  // anonymous namespace
