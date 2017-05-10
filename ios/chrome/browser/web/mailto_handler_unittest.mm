// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/web/mailto_handler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

TEST(MailtoHandlerTest, TestConstructor) {
  MailtoHandler* handler =
      [[MailtoHandler alloc] initWithName:@"Some App" appStoreID:@"12345"];
  EXPECT_NSEQ(@"Some App", [handler appName]);
  EXPECT_NSEQ(@"12345", [handler appStoreID]);
  EXPECT_NSEQ(@"mailtohandler:/co?", [handler beginningScheme]);
  EXPECT_GT([[handler supportedHeaders] count], 0U);
}

TEST(MailtoHandlerTest, TestRewriteGood) {
  MailtoHandler* handler = [[MailtoHandler alloc] init];
  // Tests mailto URL without a subject.
  NSString* result = [handler rewriteMailtoURL:GURL("mailto:user@domain.com")];
  EXPECT_NSEQ(@"mailtohandler:/co?to=user@domain.com", result);
  // Tests mailto URL with a subject.
  result =
      [handler rewriteMailtoURL:GURL("mailto:user@domain.com?subject=hello")];
  EXPECT_NSEQ(@"mailtohandler:/co?to=user@domain.com&subject=hello", result);
  // Tests mailto URL with unrecognized query parameters.
  result = [handler
      rewriteMailtoURL:
          GURL("mailto:someone@there.com?garbage=in&garbageOut&subject=trash")];
  EXPECT_NSEQ(@"mailtohandler:/co?to=someone@there.com&subject=trash", result);
}

TEST(MailtoHandlerTest, TestRewriteBad) {
  MailtoHandler* handler = [[MailtoHandler alloc] init];
  NSString* result = [handler rewriteMailtoURL:GURL("http://www.google.com")];
  EXPECT_FALSE(result);
  result = [handler
      rewriteMailtoURL:
          GURL("mailto:user@domain.com?foo=bar&cc=someone@somewhere.com")];
  EXPECT_NSEQ(@"mailtohandler:/co?to=user@domain.com&cc=someone@somewhere.com",
              result);
}
