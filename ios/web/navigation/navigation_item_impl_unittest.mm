// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_impl.h"

#include <memory>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

#ifndef NDEBUG
// Tests that the debug description is as expected.
TEST_F(NavigationItemTest, Description) {
  item_->SetTitle(base::UTF8ToUTF16("Title"));
  EXPECT_NSEQ(@"url:http://init.test/ originalurl:http://init.test/ referrer:  "
              @"title:Title transition:2 displayState:{ scrollOffset:(nan, "
              @"nan), zoomScaleRange:(nan, nan), zoomScale:nan } "
              @"userAgentType:MOBILE is_create_from_push_state: false "
              @"has_state_been_replaced: false is_created_from_hash_change: "
              @"false navigation_initiation_type: 0",
              item_->GetDescription());
}
#endif

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

  // Ensure that HTTP headers are still mutable after the copying.
  copy.AddHttpRequestHeaders(@{});
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

// Tests the behavior of GetVirtualURL().
TEST_F(NavigationItemTest, VirtualURLTest) {
  // Ensure that GetVirtualURL() returns GetURL() when not set to a custom
  // value.
  GURL original_url = item_->GetURL();
  EXPECT_EQ(original_url, item_->GetVirtualURL());
  // Set the virtual URL and check that the correct value is reported and that
  // GetURL() still reports the original URL.
  GURL new_virtual_url = GURL("http://new_url.test");
  item_->SetVirtualURL(new_virtual_url);
  EXPECT_EQ(new_virtual_url, item_->GetVirtualURL());
  EXPECT_EQ(original_url, item_->GetURL());
}

// Tests NavigationItemImpl::GetDisplayTitleForURL method.
TEST_F(NavigationItemTest, GetDisplayTitleForURL) {
  base::string16 title;

  title = NavigationItemImpl::GetDisplayTitleForURL(GURL("http://foo.org/"));
  EXPECT_EQ("foo.org", base::UTF16ToUTF8(title));

  title = NavigationItemImpl::GetDisplayTitleForURL(GURL("file://foo.org/"));
  EXPECT_EQ("file://foo.org/", base::UTF16ToUTF8(title));

  title = NavigationItemImpl::GetDisplayTitleForURL(GURL("file://foo/1.gz"));
  EXPECT_EQ("1.gz", base::UTF16ToUTF8(title));
}

}  // namespace
}  // namespace web
