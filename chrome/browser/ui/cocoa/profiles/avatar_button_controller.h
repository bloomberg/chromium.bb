// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_BUTTON_CONTROLLER_H_

#import <AppKit/AppKit.h>

#import "chrome/browser/ui/cocoa/profiles/avatar_base_controller.h"

class Browser;

// This view controller manages the button that sits in the top of the
// window frame when using multi-profiles, and shows the current profile's
// name. Clicking the button will open the profile menu.
@interface AvatarButtonController : AvatarBaseController {
 @private
  BOOL isThemedWindow_;
}
// Designated initializer.
- (id)initWithBrowser:(Browser*)browser;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_BUTTON_CONTROLLER_H_
