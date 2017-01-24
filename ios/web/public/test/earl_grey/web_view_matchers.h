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
id<GREYMatcher> WebViewInWebState(WebState* web_state);

// Matcher for WKWebView containing |text|.
id<GREYMatcher> WebViewContainingText(std::string text, WebState* web_state);

// Matcher for WKWebView not containing |text|.  This should be used to verify
// that a visible WKWebView does not contain |text|, rather than verifying that
// a WKWebView containing |text| is not visible, as would be the case if
// WebViewContainingText() were asserted with grey_nil().
id<GREYMatcher> WebViewNotContainingText(std::string text, WebState* web_state);

// Matcher for WKWebView containing a blocked |image_id|.  When blocked, the
// image element will be smaller actual image size.
id<GREYMatcher> WebViewContainingBlockedImage(std::string image_id,
                                              WebState* web_state);

// Matcher for WKWebView containing loaded image with |image_id|.  When loaded,
// the image element will have the same size as actual image.
id<GREYMatcher> WebViewContainingLoadedImage(std::string image_id,
                                             WebState* web_state);

// Matcher for WKWebView containing an html element which matches |selector|.
id<GREYMatcher> WebViewCssSelector(std::string selector, WebState* web_state);

// Matcher for WKWebView's scroll view.
id<GREYMatcher> WebViewScrollView(WebState* web_state);

// Matcher for an interstitial page. Does not wait if the page is not displayed.
id<GREYMatcher> Interstitial(WebState* web_state);

// Matcher for interstitial page containing |text|. Waits until the text is
// displayed or timed out.
id<GREYMatcher> InterstitialContainingText(NSString* text, WebState* web_state);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_TEST_EARL_GREY_WEB_VIEW_MATCHERS_H_
