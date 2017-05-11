// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_

@class ContentSuggestionsItem;
class GURL;

// Commands protocol for the ContentSuggestionsViewController.
@protocol ContentSuggestionsCommands

// Opens the Reading List.
- (void)openReadingList;
// Opens the |URL|.
- (void)openURL:(const GURL&)URL;
// Displays a context menu for opening the |articleItem|.
- (void)displayContextMenuForArticle:(ContentSuggestionsItem*)articleItem
                             atPoint:(CGPoint)touchLocation
                         atIndexPath:(NSIndexPath*)indexPath;
// Dismisses the context menu if it is displayed.
- (void)dismissContextMenu;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COMMANDS_H_
