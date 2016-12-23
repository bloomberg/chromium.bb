// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_

#include <string>

#import <EarlGrey/EarlGrey.h>

namespace chrome_test_util {

// Matcher for element with accessbitility label corresponding to |message_id|
// and acessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> buttonWithAccessibilityLabelId(int message_id);

// Matcher for element with accessbitility label corresponding to |label| and
// acessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> buttonWithAccessibilityLabel(NSString* label);

// Matcher for element with accessbitility label corresponding to |message_id|
// and acessibility trait UIAccessibilityTraitStaticText.
id<GREYMatcher> staticTextWithAccessibilityLabelId(int message_id);

// Matcher for element with accessbitility label corresponding to |label| and
// acessibility trait UIAccessibilityTraitStaticText.
id<GREYMatcher> staticTextWithAccessibilityLabel(NSString* label);

// Returns matcher for webview owned by a web controller.
id<GREYMatcher> webViewBelongingToWebController();

// Returns matcher for webview containing |text|.
id<GREYMatcher> webViewContainingText(std::string text);

// Returns matcher for webview not containing |text|.
id<GREYMatcher> webViewNotContainingText(std::string text);

// Returns matcher for a StaticHtmlViewController containing |text|.
id<GREYMatcher> staticHtmlViewContainingText(NSString* text);

// Matcher for the navigate forward button.
id<GREYMatcher> forwardButton();

// Matcher for the navigate backward button.
id<GREYMatcher> backButton();

// Matcher for the reload button.
id<GREYMatcher> reloadButton();

// Matcher for the stop loading button.
id<GREYMatcher> stopButton();

// Returns a matcher for the omnibox.
id<GREYMatcher> omnibox();

// Returns a matcher for the page security info button.
id<GREYMatcher> pageSecurityInfoButton();

// Returns matcher for omnibox containing |text|. Performs an exact match of the
// omnibox contents.
id<GREYMatcher> omniboxText(std::string text);

// Matcher for Tools menu button.
id<GREYMatcher> toolsMenuButton();

// Matcher for the Share menu button.
id<GREYMatcher> shareButton();

// Matcher for show tabs button.
id<GREYMatcher> showTabsButton();

// Matcher for CollectionViewSwitchCell.
id<GREYMatcher> collectionViewSwitchCell(NSString* accessibilityIdentifier,
                                         BOOL isOn);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
