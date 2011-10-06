// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_AVATAR_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_AVATAR_BUTTON_H_
#pragma once

#import <AppKit/AppKit.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

class Browser;

namespace AvatarButtonInternal {
class Observer;
}

// The AvatarButton sits in the top of the window frame when using multi-
// profiles. It shows the current profile's avatar, or, when in Incognito, the
// spy dude. With multi-profiles, clicking will open the profile menu; in
// Incognito, clicking will do nothing.
@interface AvatarButton : NSView {
 @private
  Browser* browser_;

  // The button child view of this view.
  scoped_nsobject<NSButton> button_;

  // Notification bridge for profile info updates.
  scoped_ptr<AvatarButtonInternal::Observer> observer_;
}

// Designated initializer.
- (id)initWithBrowser:(Browser*)browser;

// Whether or not to open the menu when clicked.
- (void)setOpenMenuOnClick:(BOOL)flag;

// Sets the image to be used as the avatar. This will have a drop shadow applied
// and will be resized to the frame of the button.
- (void)setImage:(NSImage*)image;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_AVATAR_BUTTON_H_
