// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BADGE_INFOBAR_BADGE_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BADGE_INFOBAR_BADGE_BUTTON_H_

#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"

// A button for an Infobar that contains a badge image.
@interface InfobarBadgeButton : ExtendedTouchTargetButton

// Gives the badge a dark gray background if |selected| is YES. Removes the
// background if |selected| is NO.
- (void)setSelected:(BOOL)selected;
// Sets the badge color to blue if |active| is YES, light gray if |active| is
// NO.
- (void)setActive:(BOOL)active;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BADGE_INFOBAR_BADGE_BUTTON_H_
