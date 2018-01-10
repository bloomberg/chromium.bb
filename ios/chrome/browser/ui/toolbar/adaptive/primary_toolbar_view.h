// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

@class ToolbarButton;
@class ToolbarButtonFactory;

// View for the primary toolbar. In an adaptive toolbar paradigm, this is the
// toolbar always presented.
@interface PrimaryToolbarView : UIView

// Top anchor at the bottom of the safeAreaLayoutGuide. Used so views don't
// overlap with the Status Bar.
@property(nonatomic, strong) NSLayoutYAxisAnchor* topSafeAnchor;
// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;
// The location bar view, containing the omnibox.
@property(nonatomic, strong) UIView* locationBarView;

// Property to get all the buttons in this view.
@property(nonatomic, readonly) NSArray<ToolbarButton*>* allButtons;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_H_
