// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"

namespace content {
class WebContents;
}

class ConstrainedWindow;

// A window controller to manage constrained windows. This class creates a
// native window and a ConstrainedWindow object.
//
// The caller can also set a embedded view object that will be managed by this
// class. If the embedded view resizes then the constrained window is resized to
// match.
@interface ConstrainedWindowController : NSWindowController {
 @private
  // Weak pointer to the constrained window that shows the picker.
  // The constrained window manages its own lifetime.
  // TODO(sail): Change to a strong pointer.
  ConstrainedWindow* constrainedWindow_;
  scoped_nsobject<NSView> embeddedView_;
}

// Creates a new constrained window attached to |parentWebContents|.
// The |embeddedView| is added inside the constrained window.
- (id)initWithParentWebContents:(content::WebContents*)parentWebContents
                   embeddedView:(NSView*)embeddedView;

@end

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_CONTROLLER_H_
