// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller.h"

#include <unordered_set>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "components/reading_list/ios/reading_list_model_impl.h"
#include "components/reading_list/ios/reading_list_model_storage.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "ios/web/public/web_state/web_state_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#pragma mark - MockFaviconService

// A mock FaviconService that emits an empty response.
class MockFaviconService : public favicon::FaviconService {
 public:
  MockFaviconService() : FaviconService(nullptr, nullptr) {}

  ~MockFaviconService() override {}

  base::CancelableTaskTracker::TaskId GetLargestRawFaviconForPageURL(
      const GURL& page_url,
      const std::vector<int>& icon_types,
      int minimum_size_in_pixels,
      const favicon_base::FaviconRawBitmapCallback& callback,
      base::CancelableTaskTracker* tracker) override {
    favicon_base::FaviconRawBitmapResult mock_result;
    return tracker->PostTask(base::ThreadTaskRunnerHandle::Get().get(),
                             FROM_HERE, base::Bind(callback, mock_result));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFaviconService);
};

#pragma mark - MockLargeIconService

// This class provides access to LargeIconService internals, using the current
// thread's task runner for testing.
class MockLargeIconService : public favicon::LargeIconService {
 public:
  explicit MockLargeIconService(MockFaviconService* mock_favicon_service)
      : LargeIconService(mock_favicon_service,
                         base::ThreadTaskRunnerHandle::Get()) {}
  ~MockLargeIconService() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLargeIconService);
};

#pragma mark - ReadingListViewControllerTest

class ReadingListViewControllerTest : public web::WebTestWithWebState {
 public:
  ReadingListViewControllerTest() {}
  ~ReadingListViewControllerTest() override {}

  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  std::unique_ptr<favicon::LargeIconService> large_icon_service_;
  std::unique_ptr<MockFaviconService> mock_favicon_service_;

  base::scoped_nsobject<ReadingListViewController>
      reading_list_view_controller_;

  // TODO(crbug.com/625617) When offline url can be opened, use a mock for the
  // readinglistdownloadservice.
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    mock_favicon_service_.reset(new MockFaviconService());

    // OCMOCK_VALUE needs a lvalues.
    web::WebState* mock_web_state = web_state();
    id tab = [OCMockObject mockForClass:[Tab class]];
    [[[tab stub] andReturnValue:OCMOCK_VALUE(mock_web_state)] webState];
    id tabModel = [OCMockObject mockForClass:[TabModel class]];
    [[[tabModel stub] andReturn:tab] currentTab];

    reading_list_model_.reset(new ReadingListModelImpl(nullptr, nullptr));
    large_icon_service_.reset(
        new MockLargeIconService(mock_favicon_service_.get()));
    reading_list_view_controller_.reset([[ReadingListViewController alloc]
                     initWithModel:reading_list_model_.get()
                          tabModel:tabModel
                  largeIconService:large_icon_service_.get()
        readingListDownloadService:nil]);
  }

  DISALLOW_COPY_AND_ASSIGN(ReadingListViewControllerTest);
};

// Tests that reading list items are displayed.
TEST_F(ReadingListViewControllerTest, DisplaysItems) {
  // Prefill some items.
  reading_list_model_->AddEntry(GURL("https://chromium.org"), "news");
  reading_list_model_->AddEntry(GURL("https://mail.chromium.org"), "mail");
  reading_list_model_->AddEntry(GURL("https://foo.bar"), "Foo");
  reading_list_model_->SetReadStatus(GURL("https://foo.bar"), true);

  // Load view.
  [reading_list_view_controller_ view];

  // There are two sections: Read and Unread.
  DCHECK(
      [reading_list_view_controller_.get().collectionView numberOfSections] ==
      2);
  // There are two unread articles.
  DCHECK([reading_list_view_controller_.get().collectionView
             numberOfItemsInSection:0] == 2);
  // There is one read article.
  DCHECK([reading_list_view_controller_.get().collectionView
             numberOfItemsInSection:1] == 1);
}

// Tests that the view controller is dismissed when Done button is pressed.
TEST_F(ReadingListViewControllerTest, GetsDismissed) {
  // Load view.
  [reading_list_view_controller_ view];

  id partialMock =
      [OCMockObject partialMockForObject:reading_list_view_controller_.get()];
  [[partialMock expect] dismiss];

  // Simulate tap on "Done" button.
  UIBarButtonItem* done =
      reading_list_view_controller_.get().navigationItem.rightBarButtonItem;
  [done.target performSelector:done.action];

  [partialMock verify];
}

// Tests that when an item is selected, the article is opened with UrlLoader and
// the view controller is dismissed.
TEST_F(ReadingListViewControllerTest, OpensItems) {
  id partialMock =
      [OCMockObject partialMockForObject:reading_list_view_controller_.get()];

  GURL url("https://chromium.org");
  reading_list_model_->AddEntry(url, "chromium");
  [reading_list_view_controller_ view];
  [[partialMock expect] dismiss];

  // Simulate touch on first cell.
  [reading_list_view_controller_.get().collectionView.delegate
                collectionView:reading_list_view_controller_.get()
                                   .collectionView
      didSelectItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]];
  web::NavigationItem* item =
      web_state()->GetNavigationManager()->GetPendingItem();

  EXPECT_EQ(item->GetURL().spec(), url.spec());
  EXPECT_FALSE(item->GetReferrer().url.is_valid());
  EXPECT_EQ(item->GetReferrer().policy,
            web::ReferrerPolicy::ReferrerPolicyDefault);
  EXPECT_TRUE(PageTransitionCoreTypeIs(
      item->GetTransitionType(),
      ui::PageTransition::PAGE_TRANSITION_AUTO_BOOKMARK));

  [partialMock verify];
}
