// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_H
#define CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_H
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#import "chrome/browser/cocoa/browser_accessibility_delegate.h"
#include "webkit/glue/webaccessibility.h"

using webkit_glue::WebAccessibility;

// BrowserAccessibility is a cocoa wrapper around the WebAccessibility
// object. The renderer converts webkit's accessibility tree into a
// WebAccessibility tree and passes it to the browser process over IPC.
// This class converts it into a format Cocoa can query.
@interface BrowserAccessibility : NSObject {
 @private
  WebAccessibility webAccessibility_;
  id<BrowserAccessibilityDelegate> delegate_;
  scoped_nsobject<NSMutableArray> children_;
  // The parent of the accessibility object.  This can be another
  // BrowserAccessibility or a RenderWidgetHostViewCocoa.
  id parent_;  // weak
}

// This creates a cocoa browser accessibility object around
// the webkit glue WebAccessibility object.  The delegate is
// used to communicate with the host renderer.  None of these
// parameters can be null.
- (id)initWithObject:(const WebAccessibility&)accessibility
            delegate:(id<BrowserAccessibilityDelegate>)delegate
              parent:(id)parent;

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

#endif // CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_H
