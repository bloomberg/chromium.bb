// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_VIEW_MATCHERS_H_
#define IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_VIEW_MATCHERS_H_

#include <string>

#import <EarlGrey/EarlGrey.h>

#import "ios/web/public/web_state/web_state.h"

namespace web {

// Matcher for WKWebView which belogs to the given |webState|.
id<GREYMatcher> webViewInWebState(WebState* web_state);

// Matcher for WKWebView containing |text|.
id<GREYMatcher> webViewContainingText(std::string text, WebState* web_state);

// Matcher for WKWebView not containing |text|.  This should be used to verify
// that a visible WKWebView does not contain |text|, rather than verifying that
// a WKWebView containing |text| is not visible, as would be the case if
// webViewContainingText() were asserted with grey_nil().
id<GREYMatcher> webViewNotContainingText(std::string text, WebState* web_state);

// Matcher for WKWebView containing a blocked |image_id|.  When blocked, the
// image will be smaller than |expected_size|.
id<GREYMatcher> webViewContainingBlockedImage(std::string image_id,
                                              CGSize expected_size,
                                              WebState* web_state);

// Matcher for WKWebView containing an html element which matches |selector|.
id<GREYMatcher> webViewCssSelector(std::string selector, WebState* web_state);

// Matcher for WKWebView's scroll view.
id<GREYMatcher> webViewScrollView(WebState* web_state);

// Matcher for interstitial page containing |text|.
id<GREYMatcher> interstitialContainingText(NSString* text, WebState* web_state);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_VIEW_MATCHERS_H_
