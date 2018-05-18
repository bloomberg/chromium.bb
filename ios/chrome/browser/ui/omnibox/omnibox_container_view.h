// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTAINER_VIEW_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTAINER_VIEW_H_

#import <UIKit/UIKit.h>

@class OmniboxTextFieldIOS;

// The location bar view is the view that is displayed in the visible "address
// bar" space of the toolbar. Everything that's located in the white rectangle
// is the location bar: the button on the left, the buttons on the right, the
// omnibox textfield.
@interface OmniboxContainerView : UIView

// Initialize the location bar with the given frame, font, text color, and tint
// color for omnibox.
- (instancetype)initWithFrame:(CGRect)frame
                         font:(UIFont*)font
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// The containted omnibox textfield.
@property(nonatomic, strong) OmniboxTextFieldIOS* textField;

// The leading button, such as the security status icon.
@property(nonatomic, strong) UIButton* leadingButton;

// Incognito status of the location bar changes the appearance, such as text
// and icon colors.
@property(nonatomic, assign) BOOL incognito;

// Hides and shows the leading button, without animation.
- (void)setLeadingButtonHidden:(BOOL)hidden;
// Enables or disables the leading button for user interaction.
- (void)setLeadingButtonEnabled:(BOOL)enabled;

// Sets the leading button's image.
- (void)setPlaceholderImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTAINER_VIEW_H_
