// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/toolbar/test_toolbar_model.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_web_state.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_delegate_ios.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_impl_ios.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/gtest_support.h"
#include "third_party/ocmock/ocmock_extensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

static const char kWebUrl[] = "http://www.chromium.org";
static const char kNativeUrl[] = "chrome://version";

class ToolbarModelImplIOSTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    chrome_browser_state_->CreateBookmarkModel(true);
    bookmarks::test::WaitForBookmarkModelToLoad(
        ios::BookmarkModelFactory::GetForBrowserState(
            chrome_browser_state_.get()));

    // Create a WebStateList that will always return the test WebState as
    // the active WebState.
    web_state_list_ = base::MakeUnique<WebStateList>(&web_state_list_delegate_);
    std::unique_ptr<ToolbarTestWebState> web_state =
        base::MakeUnique<ToolbarTestWebState>();
    web_state->SetBrowserState(chrome_browser_state_.get());
    web_state_ = web_state.get();
    web_state_list_->InsertWebState(0, std::move(web_state));
    web_state_list_->ActivateWebStateAt(0);

    toolbarModelDelegate_.reset(
        new ToolbarModelDelegateIOS(web_state_list_.get()));
    toolbarModel_.reset(new ToolbarModelImplIOS(toolbarModelDelegate_.get()));
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;
  ToolbarTestWebState* web_state_;
  std::unique_ptr<ToolbarModelDelegateIOS> toolbarModelDelegate_;
  std::unique_ptr<ToolbarModelIOS> toolbarModel_;
};

TEST_F(ToolbarModelImplIOSTest, TestWhenCurrentWebStateIsNull) {
  // The test fixture adds one WebState to the WebStateList, so remove it before
  // running this test.
  ASSERT_EQ(1, web_state_list_->count());
  std::unique_ptr<web::WebState> closed_web_state(
      web_state_list_->DetachWebStateAt(0));
  ASSERT_TRUE(web_state_list_->empty());

  EXPECT_FALSE(toolbarModel_->IsLoading());
  EXPECT_EQ(0, toolbarModel_->GetLoadProgressFraction());
  EXPECT_FALSE(toolbarModel_->CanGoBack());
  EXPECT_FALSE(toolbarModel_->CanGoForward());
  EXPECT_FALSE(toolbarModel_->IsCurrentTabNativePage());
  EXPECT_FALSE(toolbarModel_->IsCurrentTabBookmarked());
}

TEST_F(ToolbarModelImplIOSTest, TestIsLoading) {
  // An active webstate that is loading.
  web_state_->SetLoading(true);
  EXPECT_TRUE(toolbarModel_->IsLoading());

  // An active webstate that is not loading.
  web_state_->SetLoading(false);
  EXPECT_FALSE(toolbarModel_->IsLoading());

  // An active webstate that is pointing at a native URL.
  web_state_->SetLoading(true);
  web_state_->SetCurrentURL(GURL(kNativeUrl));
  EXPECT_FALSE(toolbarModel_->IsLoading());
}

TEST_F(ToolbarModelImplIOSTest, TestGetLoadProgressFraction) {
  const CGFloat kExpectedProgress = 0.42;
  web_state_->set_loading_progress(kExpectedProgress);
  EXPECT_FLOAT_EQ(kExpectedProgress, toolbarModel_->GetLoadProgressFraction());
}

TEST_F(ToolbarModelImplIOSTest, TestCanGoBack) {
  web_state_->SetNavigationManager(
      base::MakeUnique<ToolbarTestNavigationManager>());
  ToolbarTestNavigationManager* manager =
      static_cast<ToolbarTestNavigationManager*>(
          web_state_->GetNavigationManager());

  manager->set_can_go_back(true);
  EXPECT_TRUE(toolbarModel_->CanGoBack());

  manager->set_can_go_back(false);
  EXPECT_FALSE(toolbarModel_->CanGoBack());
}

TEST_F(ToolbarModelImplIOSTest, TestCanGoForward) {
  web_state_->SetNavigationManager(
      base::MakeUnique<ToolbarTestNavigationManager>());
  ToolbarTestNavigationManager* manager =
      static_cast<ToolbarTestNavigationManager*>(
          web_state_->GetNavigationManager());

  manager->set_can_go_forward(true);
  EXPECT_TRUE(toolbarModel_->CanGoForward());

  manager->set_can_go_forward(false);
  EXPECT_FALSE(toolbarModel_->CanGoForward());
}

TEST_F(ToolbarModelImplIOSTest, TestIsCurrentTabNativePage) {
  web_state_->SetCurrentURL(GURL(kNativeUrl));
  EXPECT_TRUE(toolbarModel_->IsCurrentTabNativePage());

  web_state_->SetCurrentURL(GURL(kWebUrl));
  EXPECT_FALSE(toolbarModel_->IsCurrentTabNativePage());
}

TEST_F(ToolbarModelImplIOSTest, TestIsCurrentTabBookmarked) {
  // Set the curent tab to |kWebUrl| and create a bookmark for |kWebUrl|, then
  // verify that the toolbar model indicates that the URL is bookmarked.
  web_state_->SetCurrentURL(GURL(kWebUrl));
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
