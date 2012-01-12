// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

class Profile;

// Manages the first run bubble.
@interface FirstRunBubbleController : BaseBubbleController {
 @private
  IBOutlet NSTextField* header_;
  Profile* profile_;
}

// Creates and shows a first run bubble.
+ (FirstRunBubbleController*) showForView:(NSView*)view
                                   offset:(NSPoint)offset
                                  profile:(Profile*)profile;

// Handle the bubble's 'Change' button; direct users to search engine options.
- (IBAction)onChange:(id)sender;

@end
