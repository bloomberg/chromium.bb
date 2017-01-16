// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/toolbar/test_toolbar_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_delegate_ios.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_impl_ios.h"
#import "ios/chrome/browser/xcallback_parameters.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"
#include "third_party/ocmock/ocmock_extensions.h"

@interface TMITestTabMock : OCMockComplexTypeHelper {
  GURL url_;
  web::WebState* web_state_;
}

@property(nonatomic, assign) const GURL& url;
@property(nonatomic, assign) web::WebState* webState;
@end

@implementation TMITestTabMock
- (const GURL&)url {
  return url_;
}
- (void)setUrl:(const GURL&)url {
  url_ = url;
}
- (web::WebState*)webState {
  return web_state_;
}
- (void)setWebState:(web::WebState*)web_state {
  web_state_ = web_state;
}
@end

namespace {

static const char kWebUrl[] = "http://www.chromium.org";
static const char kNativeUrl[] = "chrome://version";

class ToolbarModelImplIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    chrome_browser_state_->CreateBookmarkModel(true);
    bookmarks::test::WaitForBookmarkModelToLoad(
        ios::BookmarkModelFactory::GetForBrowserState(
            chrome_browser_state_.get()));

    tabModel_.reset([[OCMockObject niceMockForClass:[TabModel class]] retain]);

    toolbarModelDelegate_.reset(new ToolbarModelDelegateIOS(tabModel_.get()));
    toolbarModel_.reset(new ToolbarModelImplIOS(toolbarModelDelegate_.get()));
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::scoped_nsobject<TabModel> tabModel_;
  std::unique_ptr<ToolbarModelDelegateIOS> toolbarModelDelegate_;
  std::unique_ptr<ToolbarModelIOS> toolbarModel_;
};

class ToolbarModelImplIOSTestWebState : public web::TestWebState {
 public:
  explicit ToolbarModelImplIOSTestWebState(web::BrowserState* browser_state)
      : browser_state_(browser_state) {}

  web::BrowserState* GetBrowserState() const override { return browser_state_; }
  double GetLoadingProgress() const override { return loading_progress_; }
  void SetLoadingProgress(double loading_progress) {
    loading_progress_ = loading_progress;
  }

 private:
  web::BrowserState* browser_state_;
  double loading_progress_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarModelImplIOSTestWebState);
};

TEST_F(ToolbarModelImplIOSTest, TestWhenCurrentTabIsNull) {
  // Make a mock to always return NULL for the current tab.
  OCMockObject* tabModelMock = static_cast<OCMockObject*>(tabModel_.get());
  [[[tabModelMock stub] andReturn:NULL] currentTab];

  EXPECT_FALSE(toolbarModel_->IsLoading());
  EXPECT_EQ(0, toolbarModel_->GetLoadProgressFraction());
  EXPECT_FALSE(toolbarModel_->CanGoBack());
  EXPECT_FALSE(toolbarModel_->CanGoForward());
  EXPECT_FALSE(toolbarModel_->IsCurrentTabNativePage());
  EXPECT_FALSE(toolbarModel_->IsCurrentTabBookmarked());
}

TEST_F(ToolbarModelImplIOSTest, TestIsLoading) {
  OCMockObject* tabModelMock = static_cast<OCMockObject*>(tabModel_.get());
  id tabMock = [[TMITestTabMock alloc]
      initWithRepresentedObject:[OCMockObject mockForClass:[Tab class]]];

  // Make mocks return a current tab with a null web state.
  [[[tabModelMock stub] andReturn:tabMock] currentTab];
  [tabMock setWebState:nullptr];
  [static_cast<TMITestTabMock*>(tabMock) setUrl:GURL(kWebUrl)];
  EXPECT_FALSE(toolbarModel_->IsLoading());

  // Make mocks return a current tab that is loading.
  web::TestWebState webState;
  [tabMock setWebState:&webState];
  webState.SetLoading(true);
  EXPECT_TRUE(toolbarModel_->IsLoading());

  // Make mocks return a current tab that is not loading.
  webState.SetLoading(false);
  EXPECT_FALSE(toolbarModel_->IsLoading());

  // Make mocks return a current tab that is pointing at a native URL.
  webState.SetLoading(true);
  [static_cast<TMITestTabMock*>(tabMock) setUrl:GURL(kNativeUrl)];
  EXPECT_FALSE(toolbarModel_->IsLoading());
}

TEST_F(ToolbarModelImplIOSTest, TestGetLoadProgressFraction) {
  ToolbarModelImplIOSTestWebState web_state(chrome_browser_state_.get());
  OCMockObject* tabModelMock = static_cast<OCMockObject*>(tabModel_.get());
  id tabMock = [[TMITestTabMock alloc]
      initWithRepresentedObject:[OCMockObject mockForClass:[Tab class]]];
  [static_cast<TMITestTabMock*>(tabMock) setWebState:&web_state];
  [[[tabModelMock stub] andReturn:tabMock] currentTab];

  const CGFloat kExpectedProgress = 0.42;
  web_state.SetLoadingProgress(kExpectedProgress);
  EXPECT_FLOAT_EQ(kExpectedProgress, toolbarModel_->GetLoadProgressFraction());
}

TEST_F(ToolbarModelImplIOSTest, TestCanGoBack) {
  OCMockObject* tabModelMock = static_cast<OCMockObject*>(tabModel_.get());
  id tabMock = [[TMITestTabMock alloc]
      initWithRepresentedObject:[OCMockObject mockForClass:[Tab class]]];
  [[[tabModelMock stub] andReturn:tabMock] currentTab];

  [[[tabMock expect] andReturnBool:true] canGoBack];
  EXPECT_TRUE(toolbarModel_->CanGoBack());

  [[[tabMock expect] andReturnBool:false] canGoBack];
  EXPECT_FALSE(toolbarModel_->CanGoBack());
}

TEST_F(ToolbarModelImplIOSTest, TestCanGoForward) {
  OCMockObject* tabModelMock = static_cast<OCMockObject*>(tabModel_.get());
  id tabMock = [[TMITestTabMock alloc]
      initWithRepresentedObject:[OCMockObject mockForClass:[Tab class]]];
  [[[tabModelMock stub] andReturn:tabMock] currentTab];

  [[[tabMock expect] andReturnBool:true] canGoForward];
  EXPECT_TRUE(toolbarModel_->CanGoForward());

  [[[tabMock expect] andReturnBool:false] canGoForward];
  EXPECT_FALSE(toolbarModel_->CanGoForward());
}

TEST_F(ToolbarModelImplIOSTest, TestIsCurrentTabNativePage) {
  OCMockObject* tabModelMock = static_cast<OCMockObject*>(tabModel_.get());
  id tabMock = [[TMITestTabMock alloc]
      initWithRepresentedObject:[OCMockObject mockForClass:[Tab class]]];
  [[[tabModelMock stub] andReturn:tabMock] currentTab];

  [tabMock setUrl:GURL(kNativeUrl)];
  EXPECT_TRUE(toolbarModel_->IsCurrentTabNativePage());

  [tabMock setUrl:GURL(kWebUrl)];
  EXPECT_FALSE(toolbarModel_->IsCurrentTabNativePage());
}

TEST_F(ToolbarModelImplIOSTest, TestIsCurrentTabBookmarked) {
  ToolbarModelImplIOSTestWebState web_state(chrome_browser_state_.get());
  OCMockObject* tabModelMock = static_cast<OCMockObject*>(tabModel_.get());
  id tabMock = [[TMITestTabMock alloc]
      initWithRepresentedObject:[OCMockObject mockForClass:[Tab class]]];
  [[[tabModelMock stub] andReturn:tabMock] currentTab];

  // Set the curent tab to |kWebUrl| and create a bookmark for |kWebUrl|, then
  // verify that the toolbar model indicates that the URL is bookmarked.
  [static_cast<TMITestTabMock*>(tabMock) setWebState:&web_state];
  [static_cast<TMITestTabMock*>(tabMock) setUrl:GURL(kWebUrl)];
  bookmarks::BookmarkModel* bookmark_model =
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_browser_state_.get());
  const bookmarks::BookmarkNode* bookmarks =
      bookmark_model->bookmark_bar_node();
  const bookmarks::BookmarkNode* node =
      bookmark_model->AddURL(bookmarks, bookmarks->child_count(),
                             base::UTF8ToUTF16(kWebUrl), GURL(kWebUrl));
  EXPECT_TRUE(toolbarModel_->IsCurrentTabBookmarked());

  // Remove the bookmark and verify the toolbar model indicates that the URL is
  // not bookmarked.
  bookmark_model->Remove(node);
  EXPECT_FALSE(toolbarModel_->IsCurrentTabBookmarked());
}

}  // namespace
