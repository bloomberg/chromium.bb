// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import <EarlGrey/EarlGrey.h>

#import "ios/web/public/web_state/web_state.h"

namespace web {

// Matcher for WKWebView which belogs to the given |webState|.
id<GREYMatcher> webViewInWebState(WebState* web_state);

// Matcher for WKWebView containing |text|.
id<GREYMatcher> webViewContainingText(std::string text, WebState* web_state);

// Matcher for WKWebView containing an html element which matches |selector|.
id<GREYMatcher> webViewCssSelector(std::string selector, WebState* web_state);

// Matcher for WKWebView's scroll view.
id<GREYMatcher> webViewScrollView(WebState* web_state);

}  // namespace web
