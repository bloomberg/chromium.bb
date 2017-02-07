// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/reading_list/ios/reading_list_entry.h"
#include "components/reading_list/ios/reading_list_model_impl.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_controller.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_collection_view_item.h"
#import "ios/chrome/browser/ui/url_loader.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#include "ui/base/page_transition_types.h"

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

#pragma mark - UrlLoader

@interface UrlLoaderStub : NSObject<UrlLoader> {
  GURL _url;
  web::Referrer _referrer;
}

@property(nonatomic, readonly) const GURL& url;
@property(nonatomic, readonly) const web::Referrer& referrer;
@property(nonatomic, assign) ui::PageTransition transition;
@property(nonatomic, assign) BOOL rendererInitiated;

@end

@implementation UrlLoaderStub

@synthesize transition = _transition;
@synthesize rendererInitiated = _rendererInitiated;

- (void)loadURL:(const GURL&)url
             referrer:(const web::Referrer&)referrer
           transition:(ui::PageTransition)transition
    rendererInitiated:(BOOL)rendererInitiated {
  _url = url;
  _referrer = referrer;
  self.transition = transition;
  self.rendererInitiated = rendererInitiated;
}

- (void)webPageOrderedOpen:(const GURL&)url
                  referrer:(const web::Referrer&)referrer
                windowName:(NSString*)windowName
              inBackground:(BOOL)inBackground
                  appendTo:(OpenPosition)appendTo {
}

- (void)webPageOrderedOpen:(const GURL&)url
                  referrer:(const web::Referrer&)referrer
                windowName:(NSString*)windowName
               inIncognito:(BOOL)inIncognito
              inBackground:(BOOL)inBackground
                  appendTo:(OpenPosition)appendTo {
}

- (void)loadSessionTab:(const sessions::SessionTab*)sessionTab {
}

- (void)loadJavaScriptFromLocationBar:(NSString*)script {
}

- (const GURL&)url {
  return _url;
}

- (const web::Referrer&)referrer {
  return _referrer;
}

@end

#pragma mark - ReadingListViewControllerTest

class ReadingListViewControllerTest : public web::WebTestWithWebState {
 public:
  ReadingListViewControllerTest() {
    loader_mock_.reset([[UrlLoaderStub alloc] init]);
    mock_favicon_service_.reset(new MockFaviconService());

    reading_list_model_.reset(new ReadingListModelImpl(nullptr, nullptr));
    large_icon_service_.reset(new favicon::LargeIconService(
        mock_favicon_service_.get(), base::ThreadTaskRunnerHandle::Get()));
    container_.reset([[ReadingListViewController alloc]
                     initWithModel:reading_list_model_.get()
                            loader:loader_mock_
                  largeIconService:large_icon_service_.get()
        readingListDownloadService:nil]);
  }
  ~ReadingListViewControllerTest() override {}

  ReadingListViewController* GetContainer() { return container_; }

  ReadingListModel* GetReadingListModel() { return reading_list_model_.get(); }
  UrlLoaderStub* GetLoaderStub() { return loader_mock_; }
  ReadingListCollectionViewController*
  GetAReadingListCollectionViewController() {
    return [[[ReadingListCollectionViewController alloc]
                     initWithModel:reading_list_model_.get()
                  largeIconService:large_icon_service_.get()
        readingListDownloadService:nil
                           toolbar:nil] autorelease];
  }

 private:
  base::scoped_nsobject<ReadingListViewController> container_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  base::scoped_nsobject<UrlLoaderStub> loader_mock_;
  std::unique_ptr<favicon::LargeIconService> large_icon_service_;
  std::unique_ptr<MockFaviconService> mock_favicon_service_;
};

// Tests that the implementation of ReadingListCollectionViewController
// openItemAtIndexPath opens the entry.
TEST_F(ReadingListViewControllerTest, OpenItem) {
  // Setup.
  GURL url("https://chromium.org");
  std::string title("Chromium");
  std::unique_ptr<ReadingListEntry> entry =
      base::MakeUnique<ReadingListEntry>(url, title);
  ReadingListModel* model = GetReadingListModel();
  model->AddEntry(url, title, reading_list::ADDED_VIA_CURRENT_APP);

  base::scoped_nsobject<ReadingListCollectionViewItem> item(
      [[ReadingListCollectionViewItem alloc]
                initWithType:0
          attributesProvider:nil
                         url:url
           distillationState:ReadingListEntry::PROCESSED]);

  // Action.
  [GetContainer() readingListCollectionViewController:
                      GetAReadingListCollectionViewController()
                                             openItem:item];

  // Tests.
  UrlLoaderStub* loader = GetLoaderStub();
  EXPECT_EQ(url, loader.url);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                                           loader.transition));
  EXPECT_EQ(NO, loader.rendererInitiated);
}
