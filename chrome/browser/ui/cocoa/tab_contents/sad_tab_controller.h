// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_CONTROLLER_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

#if defined(__OBJC__)
#import <Cocoa/Cocoa.h>
#endif  // __OBJC__

namespace content {
class WebContents;
}

#if defined(__OBJC__)

// A controller class that manages the SadTabView (aka "Aw Snap" or crash page).
@interface SadTabController : NSViewController {
 @private
  content::WebContents* webContents_;  // Weak reference.
}

// Designated initializer.
- (id)initWithWebContents:(content::WebContents*)webContents;

// This action just calls the NSApp sendAction to get it into the standard
// Cocoa action processing.
- (IBAction)openLearnMoreAboutCrashLink:(id)sender;

// Returns a weak reference to the WebContents whose WebContentsView created
// this SadTabController.
- (content::WebContents*)webContents;

@end

#else

class SadTabController;

#endif  // __OBJC__

// Functions that may be accessed from non-Objective-C C/C++ code.
namespace sad_tab_controller_mac {
SadTabController* CreateSadTabController(content::WebContents* web_contents);
void RemoveSadTab(SadTabController* sad_tab);
}

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_SAD_TAB_CONTROLLER_H_
