// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TODAY_EXTENSION_NOTIFICATION_CENTER_BUTTON_H_
#define IOS_CHROME_TODAY_EXTENSION_NOTIFICATION_CENTER_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/today_extension/transparent_button.h"

// A button with a separated view background. This is needed as background
// objects in notification center must be subviews of a UIVisualEffectView.
@interface NotificationCenterButton : TransparentButton

// Designated initializer.
// Initializes a button with |title|, normal icon with asset named |icon|,
// calling [target action] on UIControlEventTouchUpInside
// event. |backgroundColor|, |inkColor|, |titleColor| are respectively the
// background inactive color, the background pressed color, and the title color.
- (instancetype)initWithTitle:(NSString*)title
                         icon:(NSString*)icon
                       target:(id)target
                       action:(SEL)action
              backgroundColor:(UIColor*)backgroundColor
                     inkColor:(UIColor*)inkColor
                   titleColor:(UIColor*)titleColor;

// Sets the different spaces of the button.
// |separator| is the distance between the left of the icon and the left of the
// text (in LTR),
// |frontShift| shifts the center of the button to the front by |frontShift|/2,
// (front is left in LTR, right in RTL).
// |horizontalPadding| sets the left and right paddings,
// |verticalPadding| sets the top and bottom paddings.
- (void)setButtonSpacesSeparator:(const CGFloat)separator
                      frontShift:(const CGFloat)frontShift
               horizontalPadding:(const CGFloat)horizontalPadding
                 verticalPadding:(const CGFloat)verticalPadding;

@end

#endif  // IOS_CHROME_TODAY_EXTENSION_NOTIFICATION_CENTER_BUTTON_H_
