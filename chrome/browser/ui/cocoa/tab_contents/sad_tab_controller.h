// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

class TabContents;

// A controller class that manages the SadTabView (aka "Aw Snap" or crash page).
@interface SadTabController : NSViewController {
 @private
  TabContents* tabContents_;  // Weak reference.
}

// Designated initializer is initWithTabContents.
- (id)initWithTabContents:(TabContents*)someTabContents
                superview:(NSView*)superview;

// This action just calls the NSApp sendAction to get it into the standard
// Cocoa action processing.
- (IBAction)openLearnMoreAboutCrashLink:(id)sender;

// Returns a weak reference to the TabContents whose TabContentsView created
// this SadTabController.
- (TabContents*)tabContents;

@end

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_CONTROLLER_H_
