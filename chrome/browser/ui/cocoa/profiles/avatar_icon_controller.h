// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_ICON_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_ICON_CONTROLLER_H_

#import <AppKit/AppKit.h>

#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/profiles/avatar_base_controller.h"

@class AvatarLabelButton;
class Browser;

// This view controller manages the button/image that sits in the top of the
// window frame when using multi-profiles. It shows the current profile's
// avatar, or, when in Incognito, the spy dude. With multi-profiles, clicking
// will open the profile menu; in Incognito, clicking will do nothing.
@interface AvatarIconController : AvatarBaseController {
 @private
  // The supervised user avatar label button. Only used for supervised user
  // profiles.
  base::scoped_nsobject<AvatarLabelButton> labelButton_;
}

// The supervised user avatar label button view.
@property(readonly, nonatomic) NSButton* labelButtonView;

// Designated initializer.
- (id)initWithBrowser:(Browser*)browser;

// Sets the image to be used as the avatar. This will have a drop shadow applied
// and will be resized to the frame of the button.
- (void)setImage:(NSImage*)image;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_ICON_CONTROLLER_H_
