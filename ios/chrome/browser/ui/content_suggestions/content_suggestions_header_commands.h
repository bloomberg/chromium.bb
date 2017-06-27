// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_COMMANDS_H_

// Commands protocol allowing the ContentSuggestionsViewController to send
// commands related to the header, containing the fake omnibox and the logo.
@protocol ContentSuggestionsHeaderCommands

// Updates the fake omnibox to adapt to the current scrolling.
- (void)updateFakeOmniboxForScrollView:(nonnull UIScrollView*)scrollView;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_COMMANDS_H_
