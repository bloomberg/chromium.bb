// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_SYNCHRONIZING_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_SYNCHRONIZING_H_

#import <UIKit/UIKit.h>

// Synchronizing protocol allowing the ContentSuggestionsViewController to
// synchronize with the header, containing the fake omnibox and the logo.
@protocol ContentSuggestionsHeaderSynchronizing

// Updates the fake omnibox to adapt to the current scrolling.
- (void)updateFakeOmniboxForScrollView:(nonnull UIScrollView*)scrollView;

// Unfocuses the omnibox.
- (void)unfocusOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_SYNCHRONIZING_H_
