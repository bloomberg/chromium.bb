// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/view_resizer.h"

class Browser;
class ExtensionShelfMac;

// A controller for the extension shelf. After creating on object of this class,
// insert its |view| into a superview and call |wasInsertedIntoWindow|. After
// that, the controller automatically registers itself in the extensions
// subsystem and manages displaying toolstrips in the extension shelf.
@interface ExtensionShelfController : NSViewController {
 @private
  CGFloat shelfHeight_;

  scoped_ptr<ExtensionShelfMac> bridge_;

  // Delegate that handles resizing our view.
  id<ViewResizer> resizeDelegate_;

  Browser* browser_;
}

// Initializes a new ExtensionShelfController.
- (id)initWithBrowser:(Browser*)browser
       resizeDelegate:(id<ViewResizer>)resizeDelegate;

// Makes the extension shelf view managed by this class visible.
- (IBAction)show:(id)sender;

// Makes the extension shelf view managed by this class invisible.
- (IBAction)hide:(id)sender;

// Returns the height this shelf has when it's visible (which is different from
// the frame's height if the shelf is hidden).
- (CGFloat)height;

// Call this once this shelf's view has been inserted into a superview. It will
// create the internal bridge object to chrome's extension system and call
// cacheDisplayInRect:toBitmapImageRep: on the |view|, which requires that it is
// in a superview.
- (void)wasInsertedIntoWindow;

@end
