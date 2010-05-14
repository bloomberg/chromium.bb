// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"

class PageInfoWindowMac;
class PrefService;
@class WindowSizeAutosaver;

// This NSWindowController subclass implements the Cocoa window for
// PageInfoWindow. This creates and owns the PageInfoWindowMac subclass.

@interface PageInfoWindowController : NSWindowController {
 @private
  // Bridge to Chromium that we own.
  scoped_ptr<PageInfoWindowMac> pageInfo_;

  scoped_nsobject<WindowSizeAutosaver> sizeSaver_;
}

// Sets the bridge between Cocoa and Chromium.
- (void)setPageInfo:(PageInfoWindowMac*)pageInfo;

// Shows the certificate display window
- (IBAction)showCertWindow:(id)sender;

@end
