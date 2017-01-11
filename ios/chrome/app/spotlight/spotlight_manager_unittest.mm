// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#import <CoreSpotlight/CoreSpotlight.h>
#import <Foundation/Foundation.h>

#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/favicon/core/favicon_client.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#import "ios/chrome/app/spotlight/bookmarks_spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_manager.h"
#import "ios/chrome/app/spotlight/spotlight_util.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/spotlight/spotlight_provider.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kDummyIconUrl[] = "http://www.example.com/touch_icon.png";

favicon_base::FaviconRawBitmapResult CreateTestBitmap(int w, int h) {
  favicon_base::FaviconRawBitmapResult result;
  result.expired = false;

  CGRect rect = CGRectMake(0, 0, w, h);
  UIGraphicsBeginImageContext(rect.size);
  CGContextRef context = UIGraphicsGetCurrentContext();
  CGContextFillRect(context, rect);
  UIImage* favicon = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();

  NSData* png = UIImagePNGRepresentation(favicon);
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes(
      static_cast<const unsigned char*>([png bytes]), [png length]));

  result.bitmap_data = data;
  result.pixel_size = gfx::Size(w, h);
  result.icon_url = GURL(kDummyIconUrl);
  result.icon_type = favicon_base::TOUCH_ICON;
  CHECK(result.is_valid());
  return result;
}

// A mock FaviconService that emits pre-programmed response.
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
    favicon_base::FaviconRawBitmapResult mock_result = CreateTestBitmap(24, 24);
    return tracker->PostTask(base::ThreadTaskRunnerHandle::Get().get(),
                             FROM_HERE, base::Bind(callback, mock_result));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFaviconService);
};

// This class provides access to LargeIconService internals, using the current
// thread's task runner for testing.
class TestLargeIconService : public favicon::LargeIconService {
 public:
  explicit TestLargeIconService(MockFaviconService* mock_favicon_service)
      : LargeIconService(mock_favicon_service,
                         base::ThreadTaskRunnerHandle::Get()) {}
  ~TestLargeIconService() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestLargeIconService);
};

class SpotlightManagerTest : public testing::Test {
 protected:
  SpotlightManagerTest() {
    mock_favicon_service_.reset(new MockFaviconService());
    large_icon_service_.reset(
        new TestLargeIconService(mock_favicon_service_.get()));
    model_ = bookmarks::TestBookmarkClient::CreateModel();
    mock_favicon_service_.reset(new MockFaviconService());
    large_icon_service_.reset(
        new TestLargeIconService(mock_favicon_service_.get()));
    bookmarksSpotlightManager_ = [[BookmarksSpotlightManager alloc]
        initWithLargeIconService:large_icon_service_.get()
                   bookmarkModel:model_.get()];
  }

  base::MessageLoop loop_;
  std::unique_ptr<MockFaviconService> mock_favicon_service_;
  std::unique_ptr<TestLargeIconService> large_icon_service_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  std::unique_ptr<bookmarks::BookmarkModel> model_;
  BookmarksSpotlightManager* bookmarksSpotlightManager_;
};

TEST_F(SpotlightManagerTest, testSpotlightID) {
  // Creating CSSearchableItem requires Spotlight to be available on the device.
  if (!spotlight::IsSpotlightAvailable())
    return;
  GURL url("http://url.com");
  NSString* spotlightId =
      [bookmarksSpotlightManager_ spotlightIDForURL:url title:@"title"];
  NSString* expectedSpotlightId =
      [NSString stringWithFormat:@"%@.9c6b643df2a0c990",
                                 spotlight::StringFromSpotlightDomain(
                                     spotlight::DOMAIN_BOOKMARKS)];
  EXPECT_NSEQ(spotlightId, expectedSpotlightId);
}

TEST_F(SpotlightManagerTest, testParentKeywordsForNode) {
  const bookmarks::BookmarkNode* root = model_->bookmark_bar_node();
  static const std::string model_string("a 1:[ b c ] d 2:[ 21:[ e ] f g ] h");
  bookmarks::test::AddNodesFromModelString(model_.get(), root, model_string);
  const bookmarks::BookmarkNode* eNode =
      root->GetChild(3)->GetChild(0)->GetChild(0);
  NSMutableArray* keywords = [[NSMutableArray alloc] init];
  [bookmarksSpotlightManager_ getParentKeywordsForNode:eNode inArray:keywords];
  EXPECT_EQ([keywords count], 2u);
  EXPECT_TRUE([[keywords objectAtIndex:0] isEqualToString:@"21"]);
  EXPECT_TRUE([[keywords objectAtIndex:1] isEqualToString:@"2"]);
}

TEST_F(SpotlightManagerTest, testBookmarksCreateSpotlightItemsWithUrl) {
  // Creating CSSearchableItem requires Spotlight to be available on the device.
  if (!spotlight::IsSpotlightAvailable())
    return;

  const bookmarks::BookmarkNode* root = model_->bookmark_bar_node();
  static const std::string model_string("a 1:[ b c ] d 2:[ 21:[ e ] f g ] h");
  bookmarks::test::AddNodesFromModelString(model_.get(), root, model_string);
  const bookmarks::BookmarkNode* eNode =
      root->GetChild(3)->GetChild(0)->GetChild(0);

  NSString* spotlightID = [bookmarksSpotlightManager_
      spotlightIDForURL:eNode->url()
                  title:base::SysUTF16ToNSString(eNode->GetTitle())];
  NSMutableArray* keywords = [[NSMutableArray alloc] init];
  [bookmarksSpotlightManager_ getParentKeywordsForNode:eNode inArray:keywords];
  NSArray* items = [bookmarksSpotlightManager_
      spotlightItemsWithURL:eNode->url()
                    favicon:nil
               defaultTitle:base::SysUTF16ToNSString(eNode->GetTitle())];
  EXPECT_TRUE([items count] == 1);
  CSSearchableItem* item = [items objectAtIndex:0];
  EXPECT_NSEQ([item uniqueIdentifier], spotlightID);
  EXPECT_NSEQ([[item attributeSet] title], @"e");
  EXPECT_NSEQ([[[item attributeSet] URL] absoluteString], @"http://e.com/");
  [bookmarksSpotlightManager_ addKeywords:keywords toSearchableItem:item];
  // We use the set intersection to verify that the item from the Spotlight
  // manager
  // contains all the newly added Keywords.
  NSMutableSet* spotlightManagerKeywords =
      [NSMutableSet setWithArray:[[item attributeSet] keywords]];
  NSSet* testModelKeywords = [NSSet setWithArray:keywords];
  [spotlightManagerKeywords intersectSet:testModelKeywords];
  EXPECT_NSEQ(spotlightManagerKeywords, testModelKeywords);
}

TEST_F(SpotlightManagerTest, testDefaultKeywordsExist) {
  // Creating CSSearchableItem requires Spotlight to be available on the device.
  if (!spotlight::IsSpotlightAvailable())
    return;

  const bookmarks::BookmarkNode* root = model_->bookmark_bar_node();
  static const std::string model_string("a 1:[ b c ] d 2:[ 21:[ e ] f g ] h");
  bookmarks::test::AddNodesFromModelString(model_.get(), root, model_string);
  const bookmarks::BookmarkNode* aNode = root->GetChild(0);
  NSArray* items = [bookmarksSpotlightManager_
      spotlightItemsWithURL:aNode->url()
                    favicon:nil
               defaultTitle:base::SysUTF16ToNSString(aNode->GetTitle())];
  EXPECT_TRUE([items count] == 1);
  CSSearchableItem* item = [items objectAtIndex:0];
  NSSet* spotlightManagerKeywords =
      [NSSet setWithArray:[[item attributeSet] keywords]];
  EXPECT_TRUE([spotlightManagerKeywords count] > 0);
  // Check static/hardcoded keywords exist
  NSSet* hardCodedKeywordsSet =
      [NSSet setWithArray:ios::GetChromeBrowserProvider()
                              ->GetSpotlightProvider()
                              ->GetAdditionalKeywords()];
  EXPECT_TRUE([hardCodedKeywordsSet isSubsetOfSet:spotlightManagerKeywords]);
}
