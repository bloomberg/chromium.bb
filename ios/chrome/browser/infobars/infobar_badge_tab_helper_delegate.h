// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_

// Delegate used by InfobarBadgeTabHelper to manage the Infobar badges.
@protocol InfobarBadgeTabHelperDelegate

// Asks the delegate to display or stop displaying a badge.
- (void)displayBadge:(BOOL)display;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_
