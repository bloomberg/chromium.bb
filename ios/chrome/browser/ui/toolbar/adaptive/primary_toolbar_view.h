// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/adaptive/adaptive_toolbar_view.h"

@class ToolbarButtonFactory;

// View for the primary toolbar. In an adaptive toolbar paradigm, this is the
// toolbar always displayed.
@interface PrimaryToolbarView : UIView<AdaptiveToolbarView>

// Initialize this View with the button |factory|. To finish the initialization
// of the view, a call to |setUp| is needed.
- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)factory
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// The location bar view, containing the omnibox.
@property(nonatomic, strong) UIView* locationBarView;

// Container for the location bar.
@property(nonatomic, strong, readonly) UIView* locationBarContainer;

// The blur visual effect view.
@property(nonatomic, strong, readwrite) UIVisualEffectView* blur;

// The height of the container for the location bar.
@property(nonatomic, strong, readonly) NSLayoutConstraint* locationBarHeight;

// StackView containing the leading buttons (relative to the location bar).
// It should only contain ToolbarButtons.
@property(nonatomic, strong, readonly) UIStackView* leadingStackView;
// StackView containing the trailing buttons (relative to the location bar).
// It should only contain ToolbarButtons.
@property(nonatomic, strong, readonly) UIStackView* trailingStackView;

// Button to cancel the edit of the location bar.
@property(nonatomic, strong, readonly) UIButton* cancelButton;

// Constraints to be activated when the location bar is focused.
@property(nonatomic, strong, readonly)
    NSMutableArray<NSLayoutConstraint*>* focusedConstraints;
// Constraints to be activated when the location bar is unfocused.
@property(nonatomic, strong, readonly)
    NSMutableArray<NSLayoutConstraint*>* unfocusedConstraints;

// Constraint for the bottom of the location bar.
@property(nonatomic, strong, readwrite)
    NSLayoutConstraint* locationBarBottomConstraint;

// Sets all the subviews and constraints of the view. The |topSafeAnchor| needs
// to be set before calling this.
- (void)setUp;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_H_
