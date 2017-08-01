// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

// Delegate for the ContentSuggestionsHeaderViewController.
@protocol ContentSuggestionsHeaderViewControllerDelegate

// Returns whether a context menu is visible.
- (BOOL)isContextMenuVisible;

// Returns whether the collection is scrolled to its top.
- (BOOL)isScrolledToTop;

@end

// Commands protocol for the header controller.
@protocol ContentSuggestionsHeaderViewControllerCommandHandler

// Dismisses all presented modals.
- (void)dismissModals;

@end

#endif  // IOS_CHROME_BROWSER_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_HEADER_VIEW_CONTROLLER_DELEGATE_H_
