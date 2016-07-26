// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import <EarlGrey/EarlGrey.h>

#import "ios/web/public/web_state/web_state.h"

namespace web {

// Shorthand for GREYMatchers::matcherForWebViewContainingText:inWebState.
id<GREYMatcher> webViewContainingText(const std::string& text,
                                      web::WebState* webState);

// Shorthand for GREYMatchers::matcherForWebWithCSSSelector:inWebState.
id<GREYMatcher> webViewCssSelector(const std::string& selector,
                                   web::WebState* webState);

// Shorthand for GREYMatchers::matcherForWebViewScrollViewInWebState.
id<GREYMatcher> webViewScrollView(web::WebState* webState);

}  // namespace web

@interface GREYMatchers (WebViewAdditions)

// Matcher for WKWebView containing |text|.
+ (id<GREYMatcher>)matcherForWebViewContainingText:(const std::string&)text
                                        inWebState:(web::WebState*)webState;

// Matcher for WKWebView containing an html element which matches |selector|.
+ (id<GREYMatcher>)matcherForWebWithCSSSelector:(const std::string&)selector
                                     inWebState:(web::WebState*)webState;

// Matcher for WKWebView's scroll view.
+ (id<GREYMatcher>)matcherForWebViewScrollViewInWebState:
    (web::WebState*)webState;

@end
