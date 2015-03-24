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
+ (web::PageScrollState)scrollStateFromDictionary:(NSDictionary*)dictionary;
+ (NSDictionary*)dictionaryFromScrollState:
    (const web::PageScrollState&)scrollState;
@end

static NSString* const kHTTPHeaderKey1 = @"key1";
static NSString* const kHTTPHeaderKey2 = @"key2";
static NSString* const kHTTPHeaderValue1 = @"value1";
static NSString* const kHTTPHeaderValue2 = @"value2";

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
    sessionEntry_.reset([[CRWSessionEntry alloc] initWithUrl:url
                                                    referrer:web::Referrer()
                                                  transition:transition
                                         useDesktopUserAgent:NO
                                           rendererInitiated:NO]);
    [sessionEntry_ navigationItem]->SetTimestamp(base::Time::Now());
    [sessionEntry_ addHTTPHeaders:@{ kHTTPHeaderKey1 : kHTTPHeaderValue1 }];
    [sessionEntry_
        setPOSTData:[@"Test data" dataUsingEncoding:NSUTF8StringEncoding]];
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
  EXPECT_EQ(entry1.index, entry2.index);
  web::NavigationItem* navItem1 = entry1.navigationItem;
  web::NavigationItem* navItem2 = entry2.navigationItem;
  // url is not compared because it could differ after copy or archive.
  EXPECT_EQ(navItem1->GetVirtualURL(), navItem2->GetVirtualURL());
  EXPECT_EQ(navItem1->GetReferrer().url, navItem2->GetReferrer().url);
  EXPECT_EQ(navItem1->GetTimestamp(), navItem2->GetTimestamp());
  EXPECT_EQ(navItem1->GetTitle(), navItem2->GetTitle());
  EXPECT_EQ(navItem1->GetPageScrollState(), navItem2->GetPageScrollState());
  EXPECT_EQ(entry1.useDesktopUserAgent, entry2.useDesktopUserAgent);
  EXPECT_EQ(entry1.usedDataReductionProxy, entry2.usedDataReductionProxy);
  EXPECT_EQ(navItem2->GetTransitionType(), transition);
  EXPECT_NSEQ(entry1.httpHeaders, entry2.httpHeaders);
  EXPECT_TRUE((!entry1.POSTData && !entry2.POSTData) ||
              [entry1.POSTData isEqualToData:entry2.POSTData]);
  EXPECT_EQ(entry1.skipResubmitDataConfirmation,
            entry2.skipResubmitDataConfirmation);
}

TEST_F(CRWSessionEntryTest, Description) {
  [sessionEntry_ navigationItem]->SetTitle(base::SysNSStringToUTF16(@"Title"));
  EXPECT_NSEQ([sessionEntry_ description], @"url:http://init.test/ "
              @"originalurl:http://init.test/ " @"title:Title " @"transition:2 "
              @"scrollState:{ scrollOffset:(nan, nan), zoomScaleRange:(nan, "
              @"nan), zoomScale:nan } " @"desktopUA:0 " @"proxy:0");
}

TEST_F(CRWSessionEntryTest, InitWithCoder) {
  web::NavigationItem* item = [sessionEntry_ navigationItem];
  item->SetVirtualURL(GURL("http://user.friendly"));
  item->SetTitle(base::SysNSStringToUTF16(@"Title"));
  int index = sessionEntry_.get().index;
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
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(index)]
      decodeIntForKey:@"index"];
  [[[decoder expect] andReturn:virtualUrl]
      decodeObjectForKey:@"virtualUrl"];
  [[[decoder expect] andReturn:referrer]
      decodeObjectForKey:@"referrer"];
  [[[decoder expect] andReturn:title]
      decodeObjectForKey:@"title"];
  const web::PageScrollState& scrollState =
      [sessionEntry_ navigationItem]->GetPageScrollState();
  NSDictionary* serializedScrollState =
      [CRWSessionEntry dictionaryFromScrollState:scrollState];
  [[[decoder expect] andReturn:serializedScrollState]
      decodeObjectForKey:@"state"];
  BOOL useDesktopUserAgent = sessionEntry_.get().useDesktopUserAgent;
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(useDesktopUserAgent)]
      decodeBoolForKey:@"useDesktopUserAgent"];
  BOOL usedDataReductionProxy = sessionEntry_.get().usedDataReductionProxy;
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(usedDataReductionProxy)]
      decodeBoolForKey:@"usedDataReductionProxy"];
  [[[decoder expect] andReturn:sessionEntry_.get().httpHeaders]
      decodeObjectForKey:@"httpHeaders"];
  [[[decoder expect] andReturn:sessionEntry_.get().POSTData]
      decodeObjectForKey:@"POSTData"];
  BOOL skipResubmitDataConfirmation =
      sessionEntry_.get().skipResubmitDataConfirmation;
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(skipResubmitDataConfirmation)]
      decodeBoolForKey:@"skipResubmitDataConfirmation"];

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
  int index = sessionEntry_.get().index;
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
      if ([key isEqualToString:@"virtualUrlString"])
        value = &virtualUrl;
      else if ([key isEqualToString:@"referrerUrlString"])
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
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(index)]
      decodeIntForKey:@"index"];
  web::ReferrerPolicy expectedPolicy = item->GetReferrer().policy;
  [[[decoder expect]
      andReturnValue:OCMOCK_VALUE(expectedPolicy)]
      decodeIntForKey:@"referrerPolicy"];
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(timestamp)]
      decodeInt64ForKey:@"timestamp"];
  [[[decoder expect] andReturn:title]
      decodeObjectForKey:@"title"];
  const web::PageScrollState& scrollState =
      [sessionEntry_ navigationItem]->GetPageScrollState();
  NSDictionary* serializedScrollState =
      [CRWSessionEntry dictionaryFromScrollState:scrollState];
  [[[decoder expect] andReturn:serializedScrollState]
      decodeObjectForKey:@"state"];
  BOOL useDesktopUserAgent = sessionEntry_.get().useDesktopUserAgent;
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(useDesktopUserAgent)]
      decodeBoolForKey:@"useDesktopUserAgent"];
  BOOL usedDataReductionProxy = sessionEntry_.get().usedDataReductionProxy;
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(usedDataReductionProxy)]
      decodeBoolForKey:@"usedDataReductionProxy"];
  [[[decoder expect] andReturn:sessionEntry_.get().httpHeaders]
      decodeObjectForKey:@"httpHeaders"];
  [[[decoder expect] andReturn:sessionEntry_.get().POSTData]
      decodeObjectForKey:@"POSTData"];
  BOOL skipResubmitDataConfirmation =
      sessionEntry_.get().skipResubmitDataConfirmation;
  [[[decoder expect] andReturnValue:OCMOCK_VALUE(skipResubmitDataConfirmation)]
      decodeBoolForKey:@"skipResubmitDataConfirmation"];

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

  encodeBytes_length_forKey_block block = ^(const uint8_t* bytes,
                                            NSUInteger length,
                                            NSString* key) {
      if ([key isEqualToString:@"virtualUrlString"]) {
        ASSERT_EQ(item->GetVirtualURL().spec(),
                  std::string(reinterpret_cast<const char*>(bytes), length));
        return;
      } else if ([key isEqualToString:@"referrerUrlString"]) {
        ASSERT_EQ(item->GetReferrer().url.spec(),
                  std::string(reinterpret_cast<const char*>(bytes), length));
        return;
      }
      FAIL();
  };
  [coder onSelector:@selector(encodeBytes:length:forKey:)
      callBlockExpectation:block];
  [[coder expect] encodeInt:[sessionEntry_ index] forKey:@"index"];
  [[coder expect] encodeInt:item->GetReferrer().policy
                     forKey:@"referrerPolicy"];
  [[coder expect] encodeInt64:item->GetTimestamp().ToInternalValue()
                       forKey:@"timestamp"];
  [[coder expect] encodeObject:title forKey:@"title"];
  const web::PageScrollState& scrollState =
      [sessionEntry_ navigationItem]->GetPageScrollState();
  NSDictionary* serializedScrollState =
      [CRWSessionEntry dictionaryFromScrollState:scrollState];
  [[coder expect] encodeObject:serializedScrollState forKey:@"state"];
  [[coder expect] encodeBool:[sessionEntry_ useDesktopUserAgent]
                      forKey:@"useDesktopUserAgent"];
  [[coder expect] encodeBool:[sessionEntry_ usedDataReductionProxy]
                      forKey:@"usedDataReductionProxy"];
  [[coder expect] encodeObject:[sessionEntry_ httpHeaders]
                        forKey:@"httpHeaders"];
  [[coder expect] encodeObject:[sessionEntry_ POSTData] forKey:@"POSTData"];
  [[coder expect] encodeBool:[sessionEntry_ skipResubmitDataConfirmation]
                      forKey:@"skipResubmitDataConfirmation"];
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

TEST_F(CRWSessionEntryTest, CreateWithNavigationItem) {
  int index = 5;  // Just pick something non-zero.
  GURL url("http://www.virtualurl.com");
  web::Referrer referrer(GURL("http://www.referrer.com"),
                         web::ReferrerPolicyDefault);
  base::string16 title = base::SysNSStringToUTF16(@"Title");
  std::string state;
  ui::PageTransition transition = ui::PAGE_TRANSITION_GENERATED;

  scoped_ptr<web::NavigationItem> navigation_item(
      new web::NavigationItemImpl());
  navigation_item->SetURL(url);
  navigation_item->SetReferrer(referrer);
  navigation_item->SetTitle(title);
  navigation_item->SetTransitionType(transition);

  base::scoped_nsobject<CRWSessionEntry> sessionEntry(
      [[CRWSessionEntry alloc] initWithNavigationItem:navigation_item.Pass()
                                                index:index]);
  web::NavigationItem* item = [sessionEntry navigationItem];
  // Validate everything was set correctly.
  EXPECT_EQ(sessionEntry.get().index, index);
  // Desktop only persists the virtual url, all three fields are initialized
  // by it.
  EXPECT_EQ(item->GetURL(), url);
  EXPECT_EQ(item->GetVirtualURL(), url);
  EXPECT_EQ(sessionEntry.get().originalUrl, url);
  EXPECT_EQ(item->GetReferrer().url, referrer.url);
  EXPECT_EQ(item->GetTitle(), title);
  EXPECT_EQ(item->GetTransitionType(), transition);
}

TEST_F(CRWSessionEntryTest, AddHTTPHeaders) {
  EXPECT_NSEQ(@{ kHTTPHeaderKey1 : kHTTPHeaderValue1 },
              [sessionEntry_ httpHeaders]);

  [sessionEntry_ addHTTPHeaders:@{ kHTTPHeaderKey1 : kHTTPHeaderValue2 }];
  EXPECT_NSEQ(@{ kHTTPHeaderKey1 : kHTTPHeaderValue2 },
              [sessionEntry_ httpHeaders]);

  [sessionEntry_ addHTTPHeaders:@{ kHTTPHeaderKey2 : kHTTPHeaderValue1 }];
  NSDictionary* expected = @{ kHTTPHeaderKey1 : kHTTPHeaderValue2,
                              kHTTPHeaderKey2 : kHTTPHeaderValue1 };
  EXPECT_NSEQ(expected, [sessionEntry_ httpHeaders]);
}

TEST_F(CRWSessionEntryTest, RemoveHTTPHeaderForKey) {
  NSDictionary* httpHeaders = @{ kHTTPHeaderKey1 : kHTTPHeaderValue1,
                                 kHTTPHeaderKey2 : kHTTPHeaderValue2 };
  [sessionEntry_ addHTTPHeaders:httpHeaders];
  EXPECT_NSEQ(httpHeaders, [sessionEntry_ httpHeaders]);

  [sessionEntry_ removeHTTPHeaderForKey:kHTTPHeaderKey1];
  EXPECT_NSEQ(@{ kHTTPHeaderKey2 : kHTTPHeaderValue2 },
              [sessionEntry_ httpHeaders]);

  [sessionEntry_ removeHTTPHeaderForKey:kHTTPHeaderKey2];
  EXPECT_TRUE([sessionEntry_ httpHeaders] == nil);
}
