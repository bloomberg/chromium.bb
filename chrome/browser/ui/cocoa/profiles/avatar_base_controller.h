// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_BASE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_BASE_CONTROLLER_H_

#import <AppKit/AppKit.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/browser_window.h"

@class BaseBubbleController;
class Browser;
class ProfileInfoUpdateObserver;

// This view controller manages the button that sits in the top of the
// window frame when using multi-profiles, and shows information about the
// current profile. Clicking the button will open the profile menu.
@interface AvatarBaseController : NSViewController {
 @protected
  Browser* browser_;

  // The avatar button. Child classes are responsible for implementing it.
  base::scoped_nsobject<NSButton> button_;

 @private
  // The menu controller, if the menu is open.
  BaseBubbleController* menuController_;

  // Observer that listens for updates to the ProfileInfoCache.
  scoped_ptr<ProfileInfoUpdateObserver> profileInfoObserver_;
}

// The avatar button view.
@property(readonly, nonatomic) NSButton* buttonView;

// Designated initializer.
- (id)initWithBrowser:(Browser*)browser;

// Shows the avatar bubble in the given mode.
- (void)showAvatarBubbleAnchoredAt:(NSView*)anchor
                          withMode:(BrowserWindow::AvatarBubbleMode)mode
                   withServiceType:(signin::GAIAServiceType)serviceType;

@end

@interface AvatarBaseController (ExposedForTesting)
- (BaseBubbleController*)menuController;
@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_BASE_CONTROLLER_H_
