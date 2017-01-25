// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_STACK_ITEM_ACTIONS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_STACK_ITEM_ACTIONS_H_

// Actions for the Suggestions Stack.
@protocol SuggestionsStackItemActions

// The item on the top of the stack have been pressed.
- (void)openReadingListFirstItem:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_STACK_ITEM_ACTIONS_H_
