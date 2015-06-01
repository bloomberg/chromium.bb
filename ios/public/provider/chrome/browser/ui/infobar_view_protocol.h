// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_INFOBAR_VIEW_PROTOCOL_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_INFOBAR_VIEW_PROTOCOL_H_

#import <Foundation/Foundation.h>

struct CGSize;
class InfoBarViewDelegate;
@class UIImage;

// Interface for setting up the infobar's widgets.
@protocol InfoBarViewProtocol
// How much of infobar is visible. The infobar is only partly visible during
// showing/hiding animation.
@property(nonatomic, assign) CGFloat visibleHeight;

// Stops propagating events to delegate.
- (void)resetDelegate;

// TODO(jif): rename methods from add* to set*. crbug.com/302582
// Adds a dismiss button subview.
- (void)addCloseButtonWithTag:(NSInteger)tag
                       target:(id)target
                       action:(SEL)action;

// Adds icon subview.
- (void)addLeftIcon:(UIImage*)image;

// Adds transparent icon of size |imageSize| as placeholder during the time when
// the icon is being downloaded.
- (void)addPlaceholderTransparentIcon:(CGSize const&)imageSize;

// Adds an icon subview with rounded corners and a shadow.
- (void)addLeftIconWithRoundedCornersAndShadow:(UIImage*)image;

// Creates a new string from |string| that is interpreted as a link by
// |addLabel:|. |tag| must not be 0.
+ (NSString*)stringAsLink:(NSString*)string tag:(NSUInteger)tag;

// Adds a message to the infobar that optionaly contains links initialized with
// |stringAsLink:|.
- (void)addLabel:(NSString*)label;

// Adds a message to the infobar that optionaly contains links initialized with
// |stringAsLink:|. |action| is called on |target| when a link is clicked.
- (void)addLabel:(NSString*)label target:(id)target action:(SEL)action;

// Adds two buttons to the infobar. Button1 is the primary action of the infobar
// and in Material Design mode is shown with bold colors to reflect this role.
- (void)addButton1:(NSString*)title1
              tag1:(NSInteger)tag1
           button2:(NSString*)title2
              tag2:(NSInteger)tag2
            target:(id)target
            action:(SEL)action;

// Adds a button to the infobar.
- (void)addButton:(NSString*)title
              tag:(NSInteger)tag
           target:(id)target
           action:(SEL)action;

// Adds to the infobar a switch and an adjacent label.
- (void)addSwitchWithLabel:(NSString*)label
                      isOn:(BOOL)isOn
                       tag:(NSInteger)tag
                    target:(id)target
                    action:(SEL)action;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_UI_INFOBAR_VIEW_PROTOCOL_H_
