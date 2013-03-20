// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"
#import "content/browser/accessibility/browser_accessibility_delegate_mac.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/accessibility_node_data.h"

// BrowserAccessibilityCocoa is a cocoa wrapper around the BrowserAccessibility
// object. The renderer converts webkit's accessibility tree into a
// WebAccessibility tree and passes it to the browser process over IPC.
// This class converts it into a format Cocoa can query.
@interface BrowserAccessibilityCocoa : NSObject {
 @private
  content::BrowserAccessibility* browserAccessibility_;
  scoped_nsobject<NSMutableArray> children_;
  id<BrowserAccessibilityDelegateCocoa> delegate_;
}

// This creates a cocoa browser accessibility object around
// the cross platform BrowserAccessibility object.  The delegate is
// used to communicate with the host renderer.  None of these
// parameters can be null.
- (id)initWithObject:(content::BrowserAccessibility*)accessibility
            delegate:(id<BrowserAccessibilityDelegateCocoa>)delegate;

// Invalidate children for a non-ignored ancestor (including self).
- (void)childrenChanged;

// Convenience method to get the internal, cross-platform role
// from browserAccessibility_.
- (content::AccessibilityNodeData::Role)internalRole;

// Return the method name for the given attribute. For testing only.
- (NSString*)methodNameForAttribute:(NSString*)attribute;

// Internally-used method.
@property(nonatomic, readonly) NSPoint origin;

// Children is an array of BrowserAccessibility objects, representing
// the accessibility children of this object.
@property(nonatomic, readonly) NSString* accessKey;
@property(nonatomic, readonly) NSNumber* ariaAtomic;
@property(nonatomic, readonly) NSNumber* ariaBusy;
@property(nonatomic, readonly) NSString* ariaLive;
@property(nonatomic, readonly) NSString* ariaRelevant;
@property(nonatomic, readonly) NSArray* children;
@property(nonatomic, readonly) NSArray* columns;
@property(nonatomic, readonly) NSString* description;
@property(nonatomic, readonly) NSNumber* disclosing;
@property(nonatomic, readonly) id disclosedByRow;
@property(nonatomic, readonly) NSNumber* disclosureLevel;
@property(nonatomic, readonly) id disclosedRows;
@property(nonatomic, readonly) NSNumber* enabled;
@property(nonatomic, readonly) NSNumber* focused;
@property(nonatomic, readonly) NSString* help;
// isIgnored returns whether or not the accessibility object
// should be ignored by the accessibility hierarchy.
@property(nonatomic, readonly, getter=isIgnored) BOOL ignored;
@property(nonatomic, readonly) NSString* invalid;
@property(nonatomic, readonly) NSNumber* loaded;
@property(nonatomic, readonly) NSNumber* loadingProgress;
@property(nonatomic, readonly) NSNumber* maxValue;
@property(nonatomic, readonly) NSNumber* minValue;
@property(nonatomic, readonly) NSNumber* numberOfCharacters;
@property(nonatomic, readonly) NSString* orientation;
@property(nonatomic, readonly) id parent;
@property(nonatomic, readonly) NSValue* position;
@property(nonatomic, readonly) NSNumber* required;
// A string indicating the role of this object as far as accessibility
// is concerned.
@property(nonatomic, readonly) NSString* role;
@property(nonatomic, readonly) NSString* roleDescription;
@property(nonatomic, readonly) NSArray* rows;
// The size of this object.
@property(nonatomic, readonly) NSValue* size;
// A string indicating the subrole of this object as far as accessibility
// is concerned.
@property(nonatomic, readonly) NSString* subrole;
// The tabs owned by a tablist.
@property(nonatomic, readonly) NSArray* tabs;
@property(nonatomic, readonly) NSString* title;
@property(nonatomic, readonly) id titleUIElement;
@property(nonatomic, readonly) NSString* url;
@property(nonatomic, readonly) NSString* value;
@property(nonatomic, readonly) NSString* valueDescription;
@property(nonatomic, readonly) NSValue* visibleCharacterRange;
@property(nonatomic, readonly) NSNumber* visited;
@property(nonatomic, readonly) id window;
@end

#endif // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
