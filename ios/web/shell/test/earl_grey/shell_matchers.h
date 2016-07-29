// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <string>

#import <EarlGrey/EarlGrey.h>

namespace web {

// Matcher for WKWebView containing |text|.
id<GREYMatcher> webViewContainingText(std::string text);

// Matcher for WKWebView containing an html element which matches |selector|.
id<GREYMatcher> webViewCssSelector(std::string selector);

// Matcher for WKWebView's scroll view.
id<GREYMatcher> webViewScrollView();

// Matcher for web shell address field text property equal to |text|.
id<GREYMatcher> addressFieldText(std::string text);

// Matcher for back button in web shell.
id<GREYMatcher> backButton();

// Matcher for forward button in web shell.
id<GREYMatcher> forwardButton();

// Matcher for address field in web shell.
id<GREYMatcher> addressField();

}  // namespace web
