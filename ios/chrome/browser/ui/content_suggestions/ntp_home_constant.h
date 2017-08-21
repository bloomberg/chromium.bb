// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_CONSTANT_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_CONSTANT_H_

#import <UIKit/UIKit.h>

namespace ntp_home {

// Returns the accessibility identifier used by the fake omnibox.
NSString* FakeOmniboxAccessibilityID();

// Distance between the Most Visited tiles and the suggestions on iPad.
extern const CGFloat kMostVisitedBottomMarginIPad;
// Distance between the Most Visited tiles and the suggestions on iPhone.
extern const CGFloat kMostVisitedBottomMarginIPhone;
// Height of the first suggestions peeking at the bottom of the screen.
extern const CGFloat kSuggestionPeekingHeight;

}  // namespace ntp_home

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_NTP_HOME_CONSTANT_H_
