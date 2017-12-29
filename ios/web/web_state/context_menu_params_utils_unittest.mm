// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/context_menu_params_utils.h"

#include "base/strings/sys_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "ios/web/public/referrer_util.h"
#import "ios/web/public/web_state/context_menu_params.h"
#import "ios/web/web_state/context_menu_constants.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Text values for the tapped element triggering the context menu.
const char kLinkUrl[] = "http://link.url/";
const char kSrcUrl[] = "http://src.url/";
const char kTitle[] = "title";
const char kReferrerPolicy[] = "always";
const char kLinkText[] = "link text";
const char kJavaScriptLinkUrl[] = "javascript://src.url/";
const char kDataUrl[] = "data://foo.bar/";
}

namespace web {

// Test fixture for error translation testing.
typedef PlatformTest ContextMenuParamsUtilsTest;

// Tests the empty contructor.
TEST_F(ContextMenuParamsUtilsTest, EmptyParams) {
  web::ContextMenuParams params;
  EXPECT_EQ(params.menu_title, nil);
  EXPECT_FALSE(params.link_url.is_valid());
  EXPECT_FALSE(params.src_url.is_valid());
  EXPECT_EQ(params.referrer_policy, web::ReferrerPolicyDefault);
  EXPECT_EQ(params.view, nil);
  EXPECT_TRUE(CGPointEqualToPoint(params.location, CGPointZero));
  EXPECT_EQ(params.link_text, nil);
}

// Tests the the parsing of the element NSDictionary.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTest) {
  web::ContextMenuParams params = web::ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementHyperlink : @(kLinkUrl),
    kContextMenuElementSource : @(kSrcUrl),
    kContextMenuElementTitle : @(kTitle),
    kContextMenuElementReferrerPolicy : @(kReferrerPolicy),
    kContextMenuElementInnerText : @(kLinkText),
  });

  EXPECT_NSEQ(params.menu_title, @(kTitle));
  EXPECT_EQ(params.link_url, GURL(kLinkUrl));
  EXPECT_EQ(params.src_url, GURL(kSrcUrl));
  EXPECT_NSEQ(params.link_text, @(kLinkText));
  EXPECT_EQ(params.referrer_policy,
            web::ReferrerPolicyFromString(kReferrerPolicy));

  EXPECT_EQ(params.view, nil);
  EXPECT_TRUE(CGPointEqualToPoint(params.location, CGPointZero));
}

// Tests title is set as the formatted URL there is no title.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTestNoTitle) {
  web::ContextMenuParams params = web::ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementHyperlink : @(kLinkUrl),
  });
  base::string16 urlText = url_formatter::FormatUrl(GURL(kLinkUrl));
  NSString* title = base::SysUTF16ToNSString(urlText);

  EXPECT_NSEQ(params.menu_title, title);
}

// Tests title is set to "JavaScript" if there is no title and "href" links to
// JavaScript URL.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTestJavascriptTitle) {
  web::ContextMenuParams params = web::ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementHyperlink : @(kJavaScriptLinkUrl),
  });
  EXPECT_NSEQ(params.menu_title, @"JavaScript");
}

// Tests title is set to |src_url| if there is no title.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTestSrcTitle) {
  web::ContextMenuParams params = web::ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementSource : @(kSrcUrl),
  });
  EXPECT_EQ(params.src_url, GURL(kSrcUrl));
  EXPECT_NSEQ(params.menu_title, @(kSrcUrl));
}

// Tests title is set to nil if there is no title and src is a data URL.
TEST_F(ContextMenuParamsUtilsTest, DictionaryConstructorTestDataTitle) {
  web::ContextMenuParams params = web::ContextMenuParamsFromElementDictionary(@{
    kContextMenuElementSource : @(kDataUrl),
  });
  EXPECT_EQ(params.src_url, GURL(kDataUrl));
  EXPECT_NSEQ(params.menu_title, nil);
}

}  // namespace web
