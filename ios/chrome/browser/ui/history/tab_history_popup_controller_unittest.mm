// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/tab_history_popup_controller.h"

#include <memory>
#include <utility>

#include "base/mac/scoped_nsobject.h"
#include "components/sessions/core/session_types.h"
#import "ios/chrome/browser/ui/history/tab_history_view_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/referrer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/gfx/ios/uikit_util.h"

@interface TabHistoryPopupController (Testing)
- (CGFloat)calculatePopupWidth:(NSArray*)entries;
@property(nonatomic, retain)
    TabHistoryViewController* tabHistoryTableViewController;
@end

namespace {
static const CGFloat kTabHistoryMinWidth = 250.0;
static const CGFloat kTabHistoryMaxWidthLandscapePhone = 350.0;

class TabHistoryPopupControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    parent_.reset([[UIView alloc] initWithFrame:[UIScreen mainScreen].bounds]);
    popup_.reset([[TabHistoryPopupController alloc]
        initWithOrigin:CGPointZero
            parentView:parent_
               entries:testEntriesArray()]);
  }
  void TearDown() override {
    parent_.reset();
    popup_.reset();
  }
  NSArray* testEntriesArray() {
    web::Referrer referrer(GURL("http://www.example.com"),
                           web::ReferrerPolicyDefault);
    std::unique_ptr<web::NavigationItem> item0 = web::NavigationItem::Create();
    item0->SetURL(GURL("http://www.example.com/0"));
    item0->SetReferrer(referrer);
    CRWSessionEntry* entry0 =
        [[CRWSessionEntry alloc] initWithNavigationItem:std::move(item0)];
    std::unique_ptr<web::NavigationItem> item1 = web::NavigationItem::Create();
    item1->SetURL(GURL("http://www.example.com/1"));
    item1->SetReferrer(referrer);
    CRWSessionEntry* entry1 =
        [[CRWSessionEntry alloc] initWithNavigationItem:std::move(item1)];
    std::unique_ptr<web::NavigationItem> item2 = web::NavigationItem::Create();
    item2->SetURL(GURL("http://www.example.com/2"));
    item2->SetReferrer(referrer);
    CRWSessionEntry* entry2 =
        [[CRWSessionEntry alloc] initWithNavigationItem:std::move(item2)];
    return [NSArray arrayWithObjects:entry0, entry1, entry2, nil];
  }
  base::scoped_nsobject<UIView> parent_;
  base::scoped_nsobject<TabHistoryPopupController> popup_;
};

TEST_F(TabHistoryPopupControllerTest, TestTableSize) {
  NSInteger number_of_rows = 0;

  UICollectionView* collectionView =
      [[popup_ tabHistoryTableViewController] collectionView];

  NSInteger number_of_sections = [collectionView numberOfSections];
  for (NSInteger section = 0; section < number_of_sections; ++section) {
    number_of_rows += [collectionView numberOfItemsInSection:section];
  }

  EXPECT_EQ(3, number_of_rows);
}

TEST_F(TabHistoryPopupControllerTest, TestCalculatePopupWidth) {
  web::Referrer referrer(GURL("http://www.example.com"),
                         web::ReferrerPolicyDefault);
  std::unique_ptr<web::NavigationItem> itemShort =
      web::NavigationItem::Create();
  itemShort->SetURL(GURL("http://foo.com/"));
  itemShort->SetReferrer(referrer);
  CRWSessionEntry* entryShort =
      [[CRWSessionEntry alloc] initWithNavigationItem:std::move(itemShort)];
  std::unique_ptr<web::NavigationItem> itemMedium =
      web::NavigationItem::Create();
  itemMedium->SetURL(GURL("http://www.example.com/mediumurl"));
  itemMedium->SetReferrer(referrer);
  CRWSessionEntry* entryMedium =
      [[CRWSessionEntry alloc] initWithNavigationItem:std::move(itemMedium)];
  std::string longURL =
      "http://www.example.com/this/is/areally/long/url/that/"
      "is/larger/than/the/maximum/table/width/so/its/text/will/get/cut/off/and/"
      "the/max/width/is/used/";
  std::unique_ptr<web::NavigationItem> itemLong = web::NavigationItem::Create();
  itemLong->SetURL(GURL(longURL));
  itemLong->SetReferrer(referrer);
  CRWSessionEntry* entryLong =
      [[CRWSessionEntry alloc] initWithNavigationItem:std::move(itemLong)];

  CGFloat minWidth = kTabHistoryMinWidth;
  CGFloat maxWidth = kTabHistoryMinWidth;
  if (!IsIPadIdiom()) {
    UIInterfaceOrientation orientation =
        [[UIApplication sharedApplication] statusBarOrientation];
    if (!UIInterfaceOrientationIsPortrait(orientation))
      maxWidth = kTabHistoryMaxWidthLandscapePhone;
  } else {
    maxWidth = ui::AlignValueToUpperPixel(
        [UIApplication sharedApplication].keyWindow.frame.size.width * .85);
  }

  CGFloat width =
      [popup_ calculatePopupWidth:[NSArray arrayWithObjects:entryShort, nil]];
  EXPECT_EQ(minWidth, width);

  width =
      [popup_ calculatePopupWidth:[NSArray arrayWithObjects:entryShort,
                                                            entryMedium, nil]];
  EXPECT_GE(width, minWidth);
  EXPECT_LE(width, maxWidth);

  width = [popup_
      calculatePopupWidth:[NSArray arrayWithObjects:entryShort, entryLong,
                                                    entryMedium, nil]];
  EXPECT_EQ(maxWidth, width);
}

}  // namespace
