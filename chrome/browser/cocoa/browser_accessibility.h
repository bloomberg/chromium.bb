// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_H
#define CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_H

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"
#include "chrome/browser/cocoa/browser_accessibility_delegate.h"
#include "webkit/glue/webaccessibility.h"

using webkit_glue::WebAccessibility;

// BrowserAccessibility is a cocoa wrapper around the WebAccessibility
// object. The renderer converts webkit's accessibility tree into a
// WebAccessibility tree and passes it to us over IPC.  This class
// converts it into a format Cocoa can query.
@interface BrowserAccessibility : NSObject {
 @private
  WebAccessibility webAccessibility_;
  id<BrowserAccessibilityDelegate> delegate_;
  scoped_nsobject<NSMutableArray> children_;
  // The parent of the accessibility object.  This can be another
  // BrowserAccessibility or a RenderWidgetHostViewCocoa.
  id parent_;
}

- (id)initWithObject:(const WebAccessibility)accessibility
            delegate:(id<BrowserAccessibilityDelegate>)delegate
              parent:(id)parent;

@property(nonatomic, readonly) NSArray* children;
@property(nonatomic, readonly, getter=isIgnored) BOOL ignored;
@property(nonatomic, readonly) NSPoint origin;
@property(nonatomic, readonly) NSString* role;
@property(nonatomic, readonly) NSSize size;

@end

#endif // CHROME_BROWSER_COCOA_BROWSER_ACCESSIBILITY_H
