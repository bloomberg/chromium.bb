// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SUGGESTIONS_SUGGESTIONS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SUGGESTIONS_SUGGESTIONS_COMMANDS_H_

// Commands protocol for the SuggestionsViewController.
@protocol SuggestionsCommands

// Adds a new empty SuggestionItem.
- (void)addEmptyItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_SUGGESTIONS_SUGGESTIONS_COMMANDS_H_
