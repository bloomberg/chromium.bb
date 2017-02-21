// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_TEXT_ITEM_ACTIONS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_TEXT_ITEM_ACTIONS_H_

// Protocol handling the actions sent in the responder chain by the suggestions
// items.
@protocol ContentSuggestionsTextItemActions

// Sent through the responder chain when the button of a suggestion item is
// pressed.
- (void)addNewTextItem:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_TEXT_ITEM_ACTIONS_H_
