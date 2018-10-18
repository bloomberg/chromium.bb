// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_CONSUMER_H_

#import <UIKit/UIKit.h>

class InfoBarIOS;

// Protocol to communicate with the Infobar container.
@protocol InfobarContainerConsumer

// Add a new infobar to the Infobar container view at position |position|.
- (void)addInfoBar:(InfoBarIOS*)infoBarIOS position:(NSInteger)position;

// Height of the frontmost infobar contained in Infobar container that is not
// hidden.
- (CGFloat)topmostVisibleInfoBarHeight;

// Animates the Infobar container alpha to |alpha|.
- (void)animateInfoBarContainerToAlpha:(CGFloat)alpha;

// Sets the Infobar container user interaction to |enabled|.
- (void)setUserInteractionEnabled:(BOOL)enabled;

// Updates Infobar container layout. This should be called when Infobar
// container needs to re-draw.
- (void)updateLayout;
@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_CONTAINER_CONSUMER_H_
