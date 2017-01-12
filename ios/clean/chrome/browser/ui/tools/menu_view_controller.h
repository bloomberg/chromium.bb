// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_MENU_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_MENU_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View controller that displays a vertical list of buttons to act as a menu.
// The view controller dismisses as soon as any of the buttons, or any area
// outside the presentation area, is tapped.
@interface MenuViewController : UIViewController
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_MENU_VIEW_CONTROLLER_H_
