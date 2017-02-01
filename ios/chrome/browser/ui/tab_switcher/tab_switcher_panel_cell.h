// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_PANEL_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_PANEL_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_model.h"

class GURL;
@class TabSwitcherCache;
@class Tab;
namespace ios {
class ChromeBrowserState;
}  // namespace ios

CGFloat tabSwitcherLocalSessionCellTopBarHeight();

@protocol SessionCellDelegate<NSObject>

- (TabSwitcherCache*)tabSwitcherCache;
- (void)cellPressed:(UICollectionViewCell*)cell;
- (void)deleteButtonPressedForCell:(UICollectionViewCell*)cell;

@end

@interface TabSwitcherSessionCell : UICollectionViewCell

// Returns the cell's identifier used for the cell's re-use.
+ (NSString*)identifier;

// The cell delegate.
@property(nonatomic, assign) id<SessionCellDelegate> delegate;

@end

// Cell showing information about a local session.
@interface TabSwitcherLocalSessionCell : TabSwitcherSessionCell

// Returns the top bar of the cell. The top bar holds the favicon and the tab
// title.
@property(nonatomic, readonly) UIView* topBar;

// Sets the cell's appearance using information in |tab|.
// The delegate needs to be set before calling this method.
- (void)setAppearanceForTab:(Tab*)tab cellSize:(CGSize)cellSize;

// PLACEHOLDER: Sets the cell's appearance using information in |title| and
// |favicon|.
- (void)setAppearanceForTabTitle:(NSString*)title
                         favicon:(UIImage*)favicon
                        cellSize:(CGSize)cellSize;

// Sets the cell's appearance depending on |type|.
- (void)setSessionType:(TabSwitcherSessionType)type;

// Returns the current screenshot set on the cell.
- (UIImage*)screenshot;
// Sets the snapshot.
- (void)setSnapshot:(UIImage*)snapshot;

@end

@interface TabSwitcherDistantSessionCell : TabSwitcherSessionCell

// Sets the cell's title.
- (void)setTitle:(NSString*)title;
// Sets the session's URL to obtain the cell's favicon.
- (void)setSessionGURL:(GURL const&)gurl
      withBrowserState:(ios::ChromeBrowserState*)browserState;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_SWITCHER_PANEL_CELL_H_
