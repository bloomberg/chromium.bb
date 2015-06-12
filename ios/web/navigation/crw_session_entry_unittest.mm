// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/public/referrer.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/page_transition_types.h"

@interface CRWSessionEntry (ExposedForTesting)
+ (web::PageScrollState)pageStateFromDictionary:(NSDictionary*)dictionary;
+ (NSDictionary*)dictionaryFromPageDisplayState:
    (const web::PageDisplayState&)displayState;
@end

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
    scoped_ptr<web::NavigationItemImpl> item(new web::NavigationItemImpl());
    item->SetURL(url);
    item->SetTransitionType(transition);
    item->SetTimestamp(base::Time::Now());
    item->SetPostData([@"Test data" dataUsingEncoding:NSUTF8StringEncoding]);
    sessionEntry_.reset(
        [[CRWSessionEntry alloc] initWithNavigationItem:item.Pass()]);
  }
  void TearDown() override { sessionEntry_.reset(); }

 protected:
  base::scoped_nsobject<CRWSessionEntry> sessionEntry_;
};

@implementation OCMockComplexTypeHelper (CRWSessionEntryTest)
typedef void (^encodeBytes_length_forKey_block)(
    const uint8_t*, NSUInteger, NSString*);
- (void)encodeBytes:(const uint8_t*)bytes
             length:(NSUInteger)length
             forKey:(NSString*)key {
  static_cast<encodeBytes_length_forKey_block>([self blockForSelector:_cmd])(
      bytes, length, key);
}

typedef const uint8_t* (^decodeBytesForKeyBlock)(NSString*, NSUInteger*);
- (const uint8_t*)decodeBytesForKey:(NSString*)key
                      returnedLength:(NSUInteger*)lengthp {
  return static_cast<decodeBytesForKeyBlock>([self blockForSelector:_cmd])(
      key, lengthp);
}
@end

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
  EXPECT_EQ(navItem1->ShouldSkipResubmitDataConfirmation(),
            navItem2->ShouldSkipResubmitDataConfirmation());
  EXPECT_EQ(navItem1->IsOverridingUserAgent(),
            navItem2->IsOverridingUserAgent());
  EXPECT_TRUE((!navItem1->HasPostData() && !navItem2->HasPostData()) ||
              [navItem1->GetPostData() isEqualToData:navItem2->GetPostData()]);
  EXPECT_EQ(navItem2->GetTransitionType(), transition);
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

TEST_F(CRWSessionEntryTest, InitWithCoder) {
  web::NavigationItem* item = [sessionEntry_ navigationItem];
  item->SetVirtualURL(GURL("http://user.friendly"));
  item->SetTitle(base::SysNSStringToUTF16(@"Title"));
  // Old serialized entries have no timestamp.
  item->SetTimestamp(base::Time::FromInternalValue(0));

  NSURL* virtualUrl = net::NSURLWithGURL(item->GetVirtualURL());
  NSURL* referrer = net::NSURLWithGURL(item->GetReferrer().url);
  NSString* title = base::SysUTF16ToNSString(item->GetTitle());
  base::scoped_nsobject<id> decoder([[OCMockComplexTypeHelper alloc]
      initWithRepresentedObject:[OCMockObject mockForClass:[NSCoder class]]]);

  decodeBytesForKeyBlock block = ^ const uint8_t* (NSString* key,
                                                   NSUInteger* length) {
      *length = 0;
      return NULL;
  };

  [[[decoder stub] andReturnValue:[NSNumber numberWithBool:NO]]
      containsValueForKey:[OCMArg any]];

  [decoder onSelector:@selector(decodeBytesForKey:returnedLength:)
      callBlockExpectation:block];
  [[[decoder expect] andReturn:virtualUrl]
      decodeObjectForKey:web::kSessionEntryURLDeperecatedKey];
  [[[decoder expect] andReturn:referrer]
      decodeObjectForKey:web::kSessionEntryReferrerURLDeprecatedKey];
  [[[decoder expect] andReturn:title]
      decodeObjectForKey:web::kSessionEntryTitleKey];
  const web::PageDisplayState& pageState =
      [sessionEntry_ navigationItem]->GetPageDisplayState();
  NSDictionary* serializedPageDisplayState =
      [CRWSessionEntry dictionaryFromPageDisplayState:pageState];
  [[[decoder expect] andReturn:serializedPageDisplayState]
      decodeObjectForKey:web::kSessionEntryPageScrollStateKey];
  BOOL useDesktopUserAgent =
      [sessionEntry_ navigationItem]->IsOverridingUserAgent();
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(useDesktopUserAgent)]
      decodeBoolForKey:web::kSessionEntryUseDesktopUserAgentKey];
  NSDictionary* requestHeaders =
      [sessionEntry_ navigationItem]->GetHttpRequestHeaders();
  [[[decoder expect] andReturn:requestHeaders]
      decodeObjectForKey:web::kSessionEntryHTTPRequestHeadersKey];
  [[[decoder expect]
      andReturn:[sessionEntry_ navigationItemImpl]->GetPostData()]
      decodeObjectForKey:web::kSessionEntryPOSTDataKey];
  BOOL skipResubmitDataConfirmation =
      [sessionEntry_ navigationItemImpl]->ShouldSkipResubmitDataConfirmation();
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(skipResubmitDataConfirmation)]
      decodeBoolForKey:web::kSessionEntrySkipResubmitConfirmationKey];

  base::scoped_nsobject<CRWSessionEntry> newSessionEntry(
      [[CRWSessionEntry alloc] initWithCoder:decoder]);
  web::NavigationItem* newItem = [newSessionEntry navigationItem];

  EXPECT_OCMOCK_VERIFY(decoder);
  expectEqualSessionEntries(sessionEntry_, newSessionEntry,
                            ui::PAGE_TRANSITION_RELOAD);
  EXPECT_NE(item->GetURL(), newItem->GetURL());
  EXPECT_EQ(item->GetVirtualURL(), newItem->GetURL());
}

TEST_F(CRWSessionEntryTest, InitWithCoderNewStyle) {
  web::NavigationItem* item = [sessionEntry_ navigationItem];
  item->SetVirtualURL(GURL("http://user.friendly"));
  item->SetTitle(base::SysNSStringToUTF16(@"Title"));
  int64 timestamp = item->GetTimestamp().ToInternalValue();

  std::string virtualUrl = item->GetVirtualURL().spec();
  std::string referrerUrl = item->GetReferrer().url.spec();
  NSString* title = base::SysUTF16ToNSString(item->GetTitle());
  base::scoped_nsobject<id> decoder([[OCMockComplexTypeHelper alloc]
      initWithRepresentedObject:[OCMockObject mockForClass:[NSCoder class]]]);

  const std::string emptyString;
  decodeBytesForKeyBlock block =  ^ const uint8_t* (NSString* key,
                                                    NSUInteger* length) {
      const std::string *value = &emptyString;
      if ([key isEqualToString:web::kSessionEntryURLKey])
        value = &virtualUrl;
      else if ([key isEqualToString:web::kSessionEntryReferrerURLKey])
        value = &referrerUrl;
      else
        EXPECT_TRUE(false);

      *length = value->size();
      return reinterpret_cast<const uint8_t*>(value->data());
  };

  [decoder onSelector:@selector(decodeBytesForKey:returnedLength:)
      callBlockExpectation:block];
  [[[decoder stub] andReturnValue:[NSNumber numberWithBool:YES]]
      containsValueForKey:[OCMArg any]];
  web::ReferrerPolicy expectedPolicy = item->GetReferrer().policy;
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(expectedPolicy)]
      decodeIntForKey:web::kSessionEntryReferrerPolicyKey];
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(timestamp)]
      decodeInt64ForKey:web::kSessionEntryTimestampKey];
  [[[decoder expect] andReturn:title]
      decodeObjectForKey:web::kSessionEntryTitleKey];
  const web::PageDisplayState& pageState =
      [sessionEntry_ navigationItem]->GetPageDisplayState();
  NSDictionary* serializedPageDisplayState =
      [CRWSessionEntry dictionaryFromPageDisplayState:pageState];
  [[[decoder expect] andReturn:serializedPageDisplayState]
      decodeObjectForKey:web::kSessionEntryPageScrollStateKey];
  BOOL useDesktopUserAgent =
      [sessionEntry_ navigationItem]->IsOverridingUserAgent();
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(useDesktopUserAgent)]
      decodeBoolForKey:web::kSessionEntryUseDesktopUserAgentKey];
  NSDictionary* requestHeaders =
      [sessionEntry_ navigationItem]->GetHttpRequestHeaders();
  [[[decoder expect] andReturn:requestHeaders]
      decodeObjectForKey:web::kSessionEntryHTTPRequestHeadersKey];
  NSData* POSTData = [sessionEntry_ navigationItemImpl]->GetPostData();
  [[[decoder expect] andReturn:POSTData]
      decodeObjectForKey:web::kSessionEntryPOSTDataKey];
  BOOL skipResubmitDataConfirmation =
      [sessionEntry_ navigationItemImpl]->ShouldSkipResubmitDataConfirmation();
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(skipResubmitDataConfirmation)]
      decodeBoolForKey:web::kSessionEntrySkipResubmitConfirmationKey];

  base::scoped_nsobject<CRWSessionEntry> newSessionEntry(
      [[CRWSessionEntry alloc] initWithCoder:decoder]);
  web::NavigationItem* newItem = [newSessionEntry navigationItem];

  EXPECT_OCMOCK_VERIFY(decoder);
  expectEqualSessionEntries(sessionEntry_, newSessionEntry,
                            ui::PAGE_TRANSITION_RELOAD);
  EXPECT_NE(item->GetURL(), newItem->GetURL());
  EXPECT_EQ(item->GetVirtualURL(), newItem->GetVirtualURL());
}

TEST_F(CRWSessionEntryTest, EncodeDecode) {
  NSData *data =
      [NSKeyedArchiver archivedDataWithRootObject:sessionEntry_];
  id decoded = [NSKeyedUnarchiver unarchiveObjectWithData:data];

  expectEqualSessionEntries(sessionEntry_, decoded,
                            ui::PAGE_TRANSITION_RELOAD);
}

TEST_F(CRWSessionEntryTest, EncodeWithCoder) {
  web::NavigationItem* item = [sessionEntry_ navigationItem];
  NSString* title = base::SysUTF16ToNSString(item->GetTitle());

  base::scoped_nsobject<id> coder([[OCMockComplexTypeHelper alloc]
      initWithRepresentedObject:[OCMockObject mockForClass:[NSCoder class]]]);

  encodeBytes_length_forKey_block block =
      ^(const uint8_t* bytes, NSUInteger length, NSString* key) {
        if ([key isEqualToString:web::kSessionEntryURLKey]) {
          ASSERT_EQ(item->GetVirtualURL().spec(),
                    std::string(reinterpret_cast<const char*>(bytes), length));
          return;
        } else if ([key isEqualToString:web::kSessionEntryReferrerURLKey]) {
          ASSERT_EQ(item->GetReferrer().url.spec(),
                    std::string(reinterpret_cast<const char*>(bytes), length));
          return;
        }
        FAIL();
      };
  [coder onSelector:@selector(encodeBytes:length:forKey:)
      callBlockExpectation:block];
  [[coder expect] encodeInt:item->GetReferrer().policy
                     forKey:web::kSessionEntryReferrerPolicyKey];
  [[coder expect] encodeInt64:item->GetTimestamp().ToInternalValue()
                       forKey:web::kSessionEntryTimestampKey];
  [[coder expect] encodeObject:title forKey:web::kSessionEntryTitleKey];
  const web::PageDisplayState& pageState =
      [sessionEntry_ navigationItem]->GetPageDisplayState();
  NSDictionary* serializedPageDisplayState =
      [CRWSessionEntry dictionaryFromPageDisplayState:pageState];
  [[coder expect] encodeObject:serializedPageDisplayState
                        forKey:web::kSessionEntryPageScrollStateKey];
  BOOL useDesktopUserAgent =
      [sessionEntry_ navigationItem]->IsOverridingUserAgent();
  [[coder expect] encodeBool:useDesktopUserAgent
                      forKey:web::kSessionEntryUseDesktopUserAgentKey];
  NSDictionary* requestHeaders =
      [sessionEntry_ navigationItem]->GetHttpRequestHeaders();
  [[coder expect] encodeObject:requestHeaders
                        forKey:web::kSessionEntryHTTPRequestHeadersKey];
  [[coder expect] encodeObject:[sessionEntry_ navigationItemImpl]->GetPostData()
                        forKey:web::kSessionEntryPOSTDataKey];
  BOOL skipResubmitDataConfirmation =
      [sessionEntry_ navigationItemImpl]->ShouldSkipResubmitDataConfirmation();
  [[coder expect] encodeBool:skipResubmitDataConfirmation
                      forKey:web::kSessionEntrySkipResubmitConfirmationKey];
  [sessionEntry_ encodeWithCoder:coder];
  EXPECT_OCMOCK_VERIFY(coder);
}

TEST_F(CRWSessionEntryTest, CodingEncoding) {
  web::NavigationItem* item = [sessionEntry_ navigationItem];
  item->SetVirtualURL(GURL("http://user.friendly"));
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:sessionEntry_];
  EXPECT_TRUE(data != nil);
  CRWSessionEntry* unarchivedSessionEntry =
      [NSKeyedUnarchiver unarchiveObjectWithData:data];
  ASSERT_TRUE(unarchivedSessionEntry != nil);
  web::NavigationItem* unarchivedItem = [unarchivedSessionEntry navigationItem];
  expectEqualSessionEntries(sessionEntry_, unarchivedSessionEntry,
                            ui::PAGE_TRANSITION_RELOAD);
  EXPECT_EQ(unarchivedItem->GetURL(), item->GetVirtualURL());
  EXPECT_NE(unarchivedItem->GetURL(), item->GetURL());
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
