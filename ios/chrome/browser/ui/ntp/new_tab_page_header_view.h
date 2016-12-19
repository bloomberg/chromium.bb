// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/toolbar_owner.h"

@protocol OmniboxFocuser;
@class TabModel;
@protocol WebToolbarDelegate;

class ReadingListModel;

// Header view for the Material Design NTP. The header view contains all views
// that are displayed above the list of most visited sites, which includes the
// toolbar buttons, Google doodle, and fake omnibox.
@interface NewTabPageHeaderView : UICollectionReusableView<ToolbarOwner>

// Return the toolbar view;
@property(nonatomic, readonly) UIView* toolBarView;

// Creates a NewTabPageToolbarController using the given |toolbarDelegate|,
// |focuser| and |readingListModel|, and adds the toolbar view to self.
- (void)addToolbarWithDelegate:(id<WebToolbarDelegate>)toolbarDelegate
                       focuser:(id<OmniboxFocuser>)focuser
                      tabModel:(TabModel*)tabModel
              readingListModel:(ReadingListModel*)readingListModel;

// Changes the frame of |searchField| based on its |initialFrame| and the scroll
// view's y |offset|. Also adjust the alpha values for |_searchBoxBorder| and
// |_shadow| and the constant values for the |constraints|.
- (void)updateSearchField:(UIView*)searchField
         withInitialFrame:(CGRect)initialFrame
       subviewConstraints:(NSArray*)constraints
                forOffset:(CGFloat)offset;

// Initializes |_searchBoxBorder| and |_shadow| and adds them to |searchField|.
- (void)addViewsToSearchField:(UIView*)searchField;

// Animates |_shadow|'s alpha to 0.
- (void)fadeOutShadow;

// Hide toolbar subviews that should not be displayed on the new tab page.
- (void)hideToolbarViewsForNewTabPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_HEADER_VIEW_H_
