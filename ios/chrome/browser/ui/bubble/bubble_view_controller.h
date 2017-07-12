// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/bubble/bubble_view.h"

#import <UIKit/UIKit.h>

// View controller that manages a BubbleView, which points to a UI element of
// interest.
@interface BubbleViewController : UIViewController

// Initializes the bubble with the given text, arrow direction, and alignment.
- (instancetype)initWithText:(NSString*)text
                   direction:(BubbleArrowDirection)arrowDirection
                   alignment:(BubbleAlignment)alignment
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Animates the bubble in with a fade-in.
- (void)animateContentIn;

// Dismisses the bubble. If |animated| is true, the bubble fades out.
- (void)dismissAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_H_
