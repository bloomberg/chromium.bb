// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_COLLAPSING_TOOLBAR_HEIGHT_CONSTRAINT_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_COLLAPSING_TOOLBAR_HEIGHT_CONSTRAINT_H_

#import <UIKit/UIKit.h>

// A constraint that scales between a collapsed and expanded height value.
@interface CollapsingToolbarHeightConstraint : NSLayoutConstraint

// Returns a constraint that manages the height of |view|.  If |view|
// conforms to the ToolbarCollapsing protocol, the collapsed and expanded
// heights are set using those return values.  Otherwise, the intrinsic height
// is used as both the collapsed and expanded height.
+ (nullable instancetype)constraintWithView:(nonnull UIView*)view;

// The collapsed and expanded toolbar heights.
@property(nonatomic, readonly) CGFloat collapsedHeight;
@property(nonatomic, readonly) CGFloat expandedHeight;

// Used to add additional height to the toolbar.
@property(nonatomic, assign) CGFloat additionalHeight;
// Whether the additional height should be collapsed.  When set to YES, the
// view's height ranges from |collapsedHeight| to |expandedHeight| +
// |additionalHeight|. When set to NO, the view's height ranges from
// |additionalHeight| + |collapsedHeight| to |additionalHeight| +
// |expandedHeight|.
@property(nonatomic, assign) BOOL collapsesAdditionalHeight;

// The interpolation progress within the height range to use for the
// constraint's constant.
@property(nonatomic, assign) CGFloat progress;

// Returns the height of the toolbar at |progress|
- (CGFloat)toolbarHeightForProgress:(CGFloat)progress;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_COLLAPSING_TOOLBAR_HEIGHT_CONSTRAINT_H_
