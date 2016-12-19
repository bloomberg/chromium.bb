// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLS_MENU_TOOLS_MENU_VIEW_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TOOLS_MENU_TOOLS_MENU_VIEW_ITEM_H_

#import <UIKit/UIKit.h>

@class ToolsMenuViewCell;

@interface ToolsMenuViewItem : NSObject
@property(nonatomic, copy) NSString* accessibilityIdentifier;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) NSInteger tag;
@property(nonatomic, assign) BOOL active;
@property(nonatomic, assign) ToolsMenuViewCell* tableViewCell;

+ (NSString*)cellID;
+ (Class)cellClass;

+ (instancetype)menuItemWithTitle:(NSString*)title
          accessibilityIdentifier:(NSString*)accessibilityIdentifier
                          command:(int)commandID;
@end

@interface ToolsMenuViewCell : UICollectionViewCell
@property(nonatomic, retain) UILabel* title;
@property(nonatomic, readonly) CGFloat horizontalMargin;

- (void)configureForMenuItem:(ToolsMenuViewItem*)item;
- (void)initializeTitleView;
- (void)initializeViews;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLS_MENU_TOOLS_MENU_VIEW_ITEM_H_
