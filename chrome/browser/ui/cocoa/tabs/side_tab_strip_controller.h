// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/tabs/tab_strip_controller.h"

// A controller for the tab strip when side tabs are enabled.
//
// TODO(pinkerton): I'm expecting there are more things here that need
// overriding rather than just tweaking a couple of settings, so I'm creating
// a full-blown subclass. Clearly, very little is actually necessary at this
// point for it to work.

@interface SideTabStripController : TabStripController {
}

@end
