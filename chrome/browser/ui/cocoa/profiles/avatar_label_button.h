// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_LABEL_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_LABEL_BUTTON_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"

@interface AvatarLabelButton : NSButton
@end

// Draws the button cell for the avatar label.
@interface AvatarLabelButtonCell : NSButtonCell

// Returns the size of the label text (including boundaries).
- (NSSize)labelTextSize;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PROFILES_AVATAR_LABEL_BUTTON_H_
