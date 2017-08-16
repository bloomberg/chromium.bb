// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_CONTROLLING_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_CONTROLLING_H_

#import <UIKit/UIKit.h>

// Controller for the ContentSuggestions header.
@protocol ContentSuggestionsHeaderControlling

// Whether the omnibox is currently focused.
@property(nonatomic, assign, getter=isOmniboxFocused, readonly)
    BOOL omniboxFocused;

// Updates the iPhone fakebox's frame based on the current scroll view |offset|
// and |width|. |width| can be 0 to use the current view width.
- (void)updateFakeOmniboxForOffset:(CGFloat)offset width:(CGFloat)width;

// Updates the fakeomnibox's width in order to be adapted to the new |width|,
// without taking the y-position into account.
- (void)updateFakeOmniboxForWidth:(CGFloat)width;

// Unfocuses the omnibox.
- (void)unfocusOmnibox;

// Calls layoutIfNeeded on the header.
- (void)layoutHeader;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_CONTROLLING_H_
