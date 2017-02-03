// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_session_entry.h"

#import <Foundation/Foundation.h>
#include <stdint.h>

#include <utility>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/public/referrer.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/page_transition_types.h"

class CRWSessionEntryTest : public PlatformTest {
 public:
  static void expectEqualSessionEntries(CRWSessionEntry* entry1,
                                        CRWSessionEntry* entry2,
                                        ui::PageTransition transition);

 protected:
  void SetUp() override {
    GURL url("http://init.test");
    ui::PageTransition transition =
        ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    std::unique_ptr<web::NavigationItemImpl> item(
        new web::NavigationItemImpl());
    item->SetOriginalRequestURL(url);
    item->SetURL(url);
    item->SetTransitionType(transition);
    item->SetTimestamp(base::Time::Now());
    item->SetPostData([@"Test data" dataUsingEncoding:NSUTF8StringEncoding]);
    sessionEntry_.reset(
        [[CRWSessionEntry alloc] initWithNavigationItem:std::move(item)]);
  }
  void TearDown() override { sessionEntry_.reset(); }

 protected:
  base::scoped_nsobject<CRWSessionEntry> sessionEntry_;
};

void CRWSessionEntryTest::expectEqualSessionEntries(
    CRWSessionEntry* entry1,
    CRWSessionEntry* entry2,
    ui::PageTransition transition) {
  web::NavigationItemImpl* navItem1 = entry1.navigationItemImpl;
  web::NavigationItemImpl* navItem2 = entry2.navigationItemImpl;
  // url is not compared because it could differ after copy or archive.
  EXPECT_EQ(navItem1->GetVirtualURL(), navItem2->GetVirtualURL());
  EXPECT_EQ(navItem1->GetReferrer().url, navItem2->GetReferrer().url);
  EXPECT_EQ(navItem1->GetTimestamp(), navItem2->GetTimestamp());
  EXPECT_EQ(navItem1->GetTitle(), navItem2->GetTitle());
  EXPECT_EQ(navItem1->GetPageDisplayState(), navItem2->GetPageDisplayState());
  EXPECT_EQ(navItem1->ShouldSkipRepostFormConfirmation(),
            navItem2->ShouldSkipRepostFormConfirmation());
  EXPECT_EQ(navItem1->IsOverridingUserAgent(),
            navItem2->IsOverridingUserAgent());
  EXPECT_TRUE((!navItem1->HasPostData() && !navItem2->HasPostData()) ||
              [navItem1->GetPostData() isEqualToData:navItem2->GetPostData()]);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      navItem2->GetTransitionType(), transition));
  EXPECT_NSEQ(navItem1->GetHttpRequestHeaders(),
              navItem2->GetHttpRequestHeaders());
}

TEST_F(CRWSessionEntryTest, Description) {
  [sessionEntry_ navigationItem]->SetTitle(base::SysNSStringToUTF16(@"Title"));
  EXPECT_NSEQ([sessionEntry_ description],
              @"url:http://init.test/ originalurl:http://init.test/ "
              @"title:Title transition:2 displayState:{ scrollOffset:(nan, "
              @"nan), zoomScaleRange:(nan, nan), zoomScale:nan } desktopUA:0");
}

TEST_F(CRWSessionEntryTest, CopyWithZone) {
  CRWSessionEntry* sessionEntry2 = [sessionEntry_ copy];
  EXPECT_NE(sessionEntry_, sessionEntry2);
  expectEqualSessionEntries(
      sessionEntry_, sessionEntry2,
      [sessionEntry_ navigationItem]->GetTransitionType());
}

TEST_F(CRWSessionEntryTest, EmptyVirtualUrl) {
  EXPECT_EQ(GURL("http://init.test/"),
            [sessionEntry_ navigationItem]->GetURL());
}

TEST_F(CRWSessionEntryTest, NonEmptyVirtualUrl) {
  web::NavigationItem* item = [sessionEntry_ navigationItem];
  item->SetVirtualURL(GURL("http://user.friendly"));
  EXPECT_EQ(GURL("http://user.friendly/"), item->GetVirtualURL());
  EXPECT_EQ(GURL("http://init.test/"), item->GetURL());
}

TEST_F(CRWSessionEntryTest, EmptyDescription) {
  EXPECT_GT([[sessionEntry_ description] length], 0U);
}
