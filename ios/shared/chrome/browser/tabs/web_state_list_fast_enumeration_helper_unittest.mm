// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/shared/chrome/browser/tabs/web_state_list_fast_enumeration_helper.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#import "ios/shared/chrome/browser/tabs/web_state_list.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kURL0[] = "https://chromium.org/0";
const char kURL1[] = "https://chromium.org/1";
const char kURL2[] = "https://chromium.org/2";
const char* const kURLs[] = {kURL0, kURL1, kURL2};
}  // namespace

@interface WebStateProxyFactoryForWebStateListFastEnumerationHelperTest
    : NSObject<WebStateProxyFactory>
@end

@implementation WebStateProxyFactoryForWebStateListFastEnumerationHelperTest

- (id)proxyForWebState:(web::WebState*)webState {
  return net::NSURLWithGURL(webState->GetVisibleURL());
}

@end

class WebStateListFastEnumerationHelperTest : public PlatformTest {
 public:
  WebStateListFastEnumerationHelperTest()
      : web_state_list_(WebStateList::WebStateOwned) {}

  NSArray* ArrayFromWebStateList() {
    id<WebStateProxyFactory> proxy =
        [[WebStateProxyFactoryForWebStateListFastEnumerationHelperTest alloc]
            init];
    WebStateListFastEnumerationHelper* helper =
        [[WebStateListFastEnumerationHelper alloc]
            initWithWebStateList:&web_state_list_
                    proxyFactory:proxy];
    NSMutableArray* array = [NSMutableArray array];
    for (id wrapper in helper) {
      // NSArray* cannot store a nil pointer. The NSFastEnumeration protocol
      // allows returning nil as one of the value in the iterated container,
      // so skip the value in that case (see NilWrapper test).
      if (wrapper)
        [array addObject:wrapper];
    }
    return [array copy];
  }

  void InsertTestWebStateForURLs(const char* const* urls, size_t count) {
    for (size_t index = 0; index < count; ++index) {
      auto test_web_state = base::MakeUnique<web::TestWebState>();
      test_web_state->SetCurrentURL(GURL(urls[index]));

      web_state_list_.InsertWebState(web_state_list_.count(),
                                     test_web_state.release(), nullptr);
    }
  }

 private:
  WebStateList web_state_list_;

  DISALLOW_COPY_AND_ASSIGN(WebStateListFastEnumerationHelperTest);
};

TEST_F(WebStateListFastEnumerationHelperTest, Empty) {
  NSArray* array = ArrayFromWebStateList();
  EXPECT_EQ(0u, [array count]);
}

TEST_F(WebStateListFastEnumerationHelperTest, FastEnumeration) {
  InsertTestWebStateForURLs(kURLs, arraysize(kURLs));
  NSArray* array = ArrayFromWebStateList();
  ASSERT_EQ(arraysize(kURLs), [array count]);
  for (size_t index = 0; index < arraysize(kURLs); ++index) {
    EXPECT_NSEQ([NSURL URLWithString:@(kURLs[index])], array[index]);
  }
}

TEST_F(WebStateListFastEnumerationHelperTest, NilWrapper) {
  const char* const urls[] = {""};
  InsertTestWebStateForURLs(urls, arraysize(urls));
  NSArray* array = ArrayFromWebStateList();
  ASSERT_EQ(0u, [array count]);
}
