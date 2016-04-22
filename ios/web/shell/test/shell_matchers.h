// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

@interface GREYMatchers (WebShellAdditions)

// Matcher for back button in web shell.
+ (id<GREYMatcher>)matcherForWebShellBackButton;

// Matcher for forward button in web shell.
+ (id<GREYMatcher>)matcherForWebShellForwardButton;

// Matcher for address field in web shell.
+ (id<GREYMatcher>)matcherForWebShellAddressField;

@end

#if !(GREY_DISABLE_SHORTHAND)

extern "C" {
// Shorthand for GREYMatchers::matcherForBackButton.
id<GREYMatcher> shell_backButton();

// Shorthand for GREYMatchers::matcherForForwardButton.
id<GREYMatcher> shell_forwardButton();

// Shorthand for GREYMatchers::matcherForAddressField.
id<GREYMatcher> shell_addressField();
}

#endif