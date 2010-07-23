// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_NEW_TAB_BUTTON
#define CHROME_BROWSER_COCOA_NEW_TAB_BUTTON

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"

// Overrides hit-test behavior to only accept clicks inside the image of the
// button, not just inside the bounding box. This could be abstracted to general
// use, but no other buttons are so irregularly shaped with respect to their
// bounding box.

@interface NewTabButton : NSButton {
 @private
  scoped_nsobject<NSBezierPath> imagePath_;
}
@end

#endif  // CHROME_BROWSER_COCOA_NEW_TAB_BUTTON
