// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/web/public/web_state/web_state.h"

namespace web {

// Shorthand for GREYMatchers::matcherForWebViewContainingText:inWebState.
id<GREYMatcher> webViewContainingText(NSString* text, web::WebState* webState);

}  // namespace web

@interface GREYMatchers (WebViewAdditions)

// Matcher for WKWebView containing |text|.
+ (id<GREYMatcher>)matcherForWebViewContainingText:(NSString*)text
                                        inWebState:(web::WebState*)webState;

@end
