// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FIRST_RUN_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_FIRST_RUN_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

class Browser;
class Profile;
@class FirstRunKeyResponder;

// Manages the first run bubble.
@interface FirstRunBubbleController : BaseBubbleController {
 @private
  IBOutlet NSTextField* header_;
  Browser* browser_;
  Profile* profile_;
  base::scoped_nsobject<FirstRunKeyResponder> keyResponder_;
}

// Creates and shows a first run bubble. |browser| is NULL in unittests.
+ (FirstRunBubbleController*) showForView:(NSView*)view
                                   offset:(NSPoint)offset
                                  browser:(Browser*)browser
                                  profile:(Profile*)profile;

// Handle the bubble's 'Change' button; direct users to search engine options.
- (IBAction)onChange:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_FIRST_RUN_BUBBLE_CONTROLLER_H_
