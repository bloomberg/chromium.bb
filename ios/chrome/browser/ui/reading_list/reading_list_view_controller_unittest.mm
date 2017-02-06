// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller.h"

#include <unordered_set>

#import "base/mac/foundation_util.h"
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
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

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

}  // namespace

#pragma mark - ReadingListViewControllerTest

class ReadingListViewControllerTest : public testing::Test {
 public:
  ReadingListViewControllerTest() {}
  ~ReadingListViewControllerTest() override {}

  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  std::unique_ptr<favicon::LargeIconService> large_icon_service_;
  std::unique_ptr<MockFaviconService> mock_favicon_service_;

  base::scoped_nsobject<ReadingListViewController>
      reading_list_view_controller_;
  id mock_delegate_;

  // TODO(crbug.com/625617) When offline url can be opened, use a mock for the
  // readinglistdownloadservice.
  void SetUp() override {
    testing::Test::SetUp();
    mock_favicon_service_.reset(new MockFaviconService());

    reading_list_model_.reset(new ReadingListModelImpl(nullptr, nullptr));
    large_icon_service_.reset(new favicon::LargeIconService(
        mock_favicon_service_.get(), base::ThreadTaskRunnerHandle::Get()));
    reading_list_view_controller_.reset([[ReadingListViewController alloc]
                     initWithModel:reading_list_model_.get()
                  largeIconService:large_icon_service_.get()
        readingListDownloadService:nil
                           toolbar:nil]);

    mock_delegate_ = [OCMockObject
        niceMockForProtocol:@protocol(ReadingListViewControllerDelegate)];
    [reading_list_view_controller_ setDelegate:mock_delegate_];
  }

 private:
  web::TestWebThreadBundle thread_bundle_;
  DISALLOW_COPY_AND_ASSIGN(ReadingListViewControllerTest);
};

// Tests that reading list items are displayed.
TEST_F(ReadingListViewControllerTest, DisplaysItems) {
  // Prefill some items.
  reading_list_model_->AddEntry(GURL("https://chromium.org"), "news",
                                reading_list::ADDED_VIA_CURRENT_APP);
  reading_list_model_->AddEntry(GURL("https://mail.chromium.org"), "mail",
                                reading_list::ADDED_VIA_CURRENT_APP);
  reading_list_model_->AddEntry(GURL("https://foo.bar"), "Foo",
                                reading_list::ADDED_VIA_CURRENT_APP);
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

  [[mock_delegate_ expect]
      dismissReadingListViewController:reading_list_view_controller_.get()];

  // Simulate tap on "Done" button.
  UIBarButtonItem* done =
      reading_list_view_controller_.get().navigationItem.rightBarButtonItem;
  [done.target performSelector:done.action];

  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}

// Tests that when an item is selected, the article is opened with UrlLoader and
// the view controller is dismissed.
TEST_F(ReadingListViewControllerTest, OpensItems) {
  NSIndexPath* indexPath = [NSIndexPath indexPathForItem:1 inSection:0];

  GURL url("https://chromium.org");
  GURL url2("https://chromium.org/2");
  reading_list_model_->AddEntry(url, "chromium",
                                reading_list::ADDED_VIA_CURRENT_APP);
  reading_list_model_->AddEntry(url2, "chromium - 2",
                                reading_list::ADDED_VIA_CURRENT_APP);

  ReadingListCollectionViewItem* readingListItem =
      base::mac::ObjCCastStrict<ReadingListCollectionViewItem>(
          [[reading_list_view_controller_ collectionViewModel]
              itemAtIndexPath:indexPath]);

  [[mock_delegate_ expect]
      readingListViewController:reading_list_view_controller_.get()
                       openItem:readingListItem];

  // Simulate touch on second cell.
  [reading_list_view_controller_
                collectionView:reading_list_view_controller_.get()
                                   .collectionView
      didSelectItemAtIndexPath:indexPath];

  EXPECT_OCMOCK_VERIFY(mock_delegate_);
}
