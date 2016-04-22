// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

@interface GREYMatchers (WebViewAdditions)

// Matcher for WKWebView containing |text|.
+ (id<GREYMatcher>)matcherForWebViewContainingText:(NSString*)text;

#if !(GREY_DISABLE_SHORTHAND)

extern "C" {
// Shorthand for GREYMatchers::matcherForWebViewContainingText.
GREY_EXPORT id<GREYMatcher> shell_webViewContainingText(NSString* text);
}

#endif

@end