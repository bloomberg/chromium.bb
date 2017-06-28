// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_UTILS_H_

#import <UIKit/UIKit.h>

@class ContentSuggestionsViewController;

// Utils for the ContentSuggestionsViewController
@interface ContentSuggestionsViewControllerUtils : NSObject

// Updates the |suggestionsViewController| when a dragging of its scroll view
// ends. It also updates the |targetContentOffset| to reflect where the scroll
// should end.
+ (void)viewControllerWillEndDragging:
            (ContentSuggestionsViewController*)suggestionsViewController
                          withYOffset:(CGFloat)yOffset
                        pinnedYOffset:(CGFloat)pinnedYOffset
                       draggingUpward:(BOOL)draggingUpward
                  targetContentOffset:(inout CGPoint*)targetContentOffset;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_VIEW_CONTROLLER_UTILS_H_
