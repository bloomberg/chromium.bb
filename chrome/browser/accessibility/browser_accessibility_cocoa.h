// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
#define CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#import "chrome/browser/accessibility/browser_accessibility_delegate_mac.h"
#include "chrome/browser/accessibility/browser_accessibility.h"

// BrowserAccessibilityCocoa is a cocoa wrapper around the BrowserAccessibility
// object. The renderer converts webkit's accessibility tree into a
// WebAccessibility tree and passes it to the browser process over IPC.
// This class converts it into a format Cocoa can query.
@interface BrowserAccessibilityCocoa : NSObject {
 @private
  BrowserAccessibility* browserAccessibility_;
  id<BrowserAccessibilityDelegateCocoa> delegate_;
}

// This creates a cocoa browser accessibility object around
// the cross platform BrowserAccessibility object.  The delegate is
// used to communicate with the host renderer.  None of these
// parameters can be null.
- (id)initWithObject:(BrowserAccessibility*)accessibility
            delegate:(id<BrowserAccessibilityDelegateCocoa>)delegate;

// Children is an array of BrowserAccessibility objects, representing
// the accessibility children of this object.
@property(nonatomic, readonly) NSArray* children;
// isIgnored returns whether or not the accessibility object
// should be ignored by the accessibility hierarchy.
@property(nonatomic, readonly, getter=isIgnored) BOOL ignored;
// The origin of this object in the page's document.
// This is relative to webkit's top-left origin, not Cocoa's
// bottom-left origin.
@property(nonatomic, readonly) NSPoint origin;
// A string indicating the role of this object as far as accessibility
// is concerned.
@property(nonatomic, readonly) NSString* role;
// The size of this object.
@property(nonatomic, readonly) NSSize size;

@end

#endif // CHROME_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
