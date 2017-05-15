// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/origin_util.h"

#import <WebKit/WebKit.h>

#include "base/mac/objc_release_properties.h"
#import "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// WKSecurityOrigin can not be manually created, hence stub is needed.
@interface WKSecurityOriginStub : NSObject
// Methods from WKSecurityOrigin class.
@property(nonatomic, copy) NSString* protocol;
@property(nonatomic, copy) NSString* host;
@property(nonatomic) NSInteger port;
@end

@implementation WKSecurityOriginStub
@synthesize protocol = _protocol;
@synthesize host = _host;
@synthesize port = _port;
- (void)dealloc {
  base::mac::ReleaseProperties(self);
  [super dealloc];
}
@end

namespace web {

// Tests calling GURLOriginWithWKSecurityOrigin with nil.
TEST(OriginUtilTest, GURLOriginWithNilWKSecurityOrigin) {
  GURL url(GURLOriginWithWKSecurityOrigin(nil));

  EXPECT_FALSE(url.is_valid());
  EXPECT_TRUE(url.spec().empty());
}

// Tests calling GURLOriginWithWKSecurityOrigin with valid origin.
TEST(OriginUtilTest, GURLOriginWithValidWKSecurityOrigin) {
  base::scoped_nsobject<WKSecurityOriginStub> origin(
      [[WKSecurityOriginStub alloc] init]);
  [origin setProtocol:@"http"];
  [origin setHost:@"chromium.org"];
  [origin setPort:80];

  GURL url(GURLOriginWithWKSecurityOrigin(
      static_cast<WKSecurityOrigin*>(origin.get())));
  EXPECT_EQ("http://chromium.org/", url.spec());
  EXPECT_TRUE(url.port().empty());
}

}  // namespace web
