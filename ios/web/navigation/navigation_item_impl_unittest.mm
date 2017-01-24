// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_impl.h"

#include <memory>

#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace web {
namespace {

const char kItemURLString[] = "http://init.test";
static NSString* const kHTTPHeaderKey1 = @"key1";
static NSString* const kHTTPHeaderKey2 = @"key2";
static NSString* const kHTTPHeaderValue1 = @"value1";
static NSString* const kHTTPHeaderValue2 = @"value2";

class NavigationItemTest : public PlatformTest {
 protected:
  void SetUp() override {
    item_.reset(new web::NavigationItemImpl());
    item_->SetOriginalRequestURL(GURL(kItemURLString));
    item_->SetURL(GURL(kItemURLString));
    item_->SetTransitionType(ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    item_->SetTimestamp(base::Time::Now());
    item_->AddHttpRequestHeaders(@{kHTTPHeaderKey1 : kHTTPHeaderValue1});
    item_->SetPostData([@"Test data" dataUsingEncoding:NSUTF8StringEncoding]);
  }

  // The NavigationItemImpl instance being tested.
  std::unique_ptr<NavigationItemImpl> item_;
};

// TODO(rohitrao): Add and adapt tests from NavigationEntryImpl.
TEST_F(NavigationItemTest, Dummy) {
  const GURL url("http://init.test");
  item_->SetURL(url);
  EXPECT_TRUE(item_->GetURL().is_valid());
}

// Tests that copied NavigationItemImpls create copies of data members that are
// objects.
TEST_F(NavigationItemTest, Copy) {
  // Create objects to be copied.
  NSString* postData0 = @"postData0";
  NSMutableData* mutablePostData =
      [[postData0 dataUsingEncoding:NSUTF8StringEncoding] mutableCopy];
  item_->SetPostData(mutablePostData);
  NSString* state0 = @"state0";
  NSMutableString* mutableState = [state0 mutableCopy];
  item_->SetSerializedStateObject(mutableState);

  // Create copy.
  web::NavigationItemImpl copy(*item_.get());

  // Modify the objects.
  NSString* postData1 = @"postData1";
  [mutablePostData setData:[postData1 dataUsingEncoding:NSUTF8StringEncoding]];
  NSString* state1 = @"state1";
  [mutableState setString:state1];

  // Check that changes occurred in |item_|, but not in |copy|.
  EXPECT_NSEQ([postData1 dataUsingEncoding:NSUTF8StringEncoding],
              item_->GetPostData());
  EXPECT_NSEQ(state1, item_->GetSerializedStateObject());
  EXPECT_NSEQ([postData0 dataUsingEncoding:NSUTF8StringEncoding],
              copy.GetPostData());
  EXPECT_NSEQ(state0, copy.GetSerializedStateObject());
}

// Tests whether |NavigationItem::AddHttpRequestHeaders()| adds the passed
// headers to the item's request http headers.
TEST_F(NavigationItemTest, AddHttpRequestHeaders) {
  EXPECT_NSEQ(@{kHTTPHeaderKey1 : kHTTPHeaderValue1},
              item_->GetHttpRequestHeaders());

  item_->AddHttpRequestHeaders(@{kHTTPHeaderKey1 : kHTTPHeaderValue2});
  EXPECT_NSEQ(@{kHTTPHeaderKey1 : kHTTPHeaderValue2},
              item_->GetHttpRequestHeaders());

  item_->AddHttpRequestHeaders(@{kHTTPHeaderKey2 : kHTTPHeaderValue1});
  NSDictionary* expected = @{
    kHTTPHeaderKey1 : kHTTPHeaderValue2,
    kHTTPHeaderKey2 : kHTTPHeaderValue1
  };
  EXPECT_NSEQ(expected, item_->GetHttpRequestHeaders());
}

// Tests whether |NavigationItem::AddHttpRequestHeaders()| removes the header
// value associated with the passed key from the item's request http headers.
TEST_F(NavigationItemTest, RemoveHttpRequestHeaderForKey) {
  NSDictionary* httpHeaders = @{
    kHTTPHeaderKey1 : kHTTPHeaderValue1,
    kHTTPHeaderKey2 : kHTTPHeaderValue2
  };
  item_->AddHttpRequestHeaders(httpHeaders);
  EXPECT_NSEQ(httpHeaders, item_->GetHttpRequestHeaders());

  item_->RemoveHttpRequestHeaderForKey(kHTTPHeaderKey1);
  EXPECT_NSEQ(@{kHTTPHeaderKey2 : kHTTPHeaderValue2},
              item_->GetHttpRequestHeaders());

  item_->RemoveHttpRequestHeaderForKey(kHTTPHeaderKey2);
  EXPECT_FALSE(item_->GetHttpRequestHeaders());
}

// Tests the getter, setter, and copy constructor for the original request URL.
TEST_F(NavigationItemTest, OriginalURL) {
  GURL original_url = GURL(kItemURLString);
  EXPECT_EQ(original_url, item_->GetOriginalRequestURL());
  web::NavigationItemImpl copy(*item_);
  GURL new_url = GURL("http://new_url.test");
  item_->SetOriginalRequestURL(new_url);
  EXPECT_EQ(new_url, item_->GetOriginalRequestURL());
  EXPECT_EQ(original_url, copy.GetOriginalRequestURL());
}

}  // namespace
}  // namespace web
