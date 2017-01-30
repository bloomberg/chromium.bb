// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CHROME_BROWSER_BROWSER_COORDINATOR_INTERNAL_H_
#define IOS_CHROME_BROWSER_BROWSER_COORDINATOR_INTERNAL_H_

#import "ios/clean/chrome/browser/browser_coordinator.h"

// Internal API for subclasses and categories of BrowserCoordinator.
//
// This API can be used to create and manage 'overlay coordinators'.
// Overlay coordinators are intended to be used for UI elements that
// sit on top of the regular UI, and which may need to be dismissed
// by coordinators that don't have direct access to them. Typical uses
// would be alerts, login prompts, or other UI that might be dismissed
// if (for example) an external event causes a web page to load.
// Overlay coordinators can be added by any coordinator in the coordinator
// hierarchy, but they will be children of (generally) the topmost
// coordinator, regardless of which coordinator added them.
@interface BrowserCoordinator (Internal)

// Managed view controller of this object. Subclasses must define a
// property named |viewController| that has the specific UIViewController
// subclass they use; the subclass API will be able to access that view
// controller through this property.
@property(nonatomic, readonly) UIViewController* viewController;

// The child coordinators of this coordinator. To add or remove from this set,
// use the -addChildCoordinator: and -removeChildCoordinator: methods.
@property(nonatomic, readonly) NSSet<BrowserCoordinator*>* children;

// The coordinator that added this coordinator as a child, if any.
@property(nonatomic, readonly) BrowserCoordinator* parentCoordinator;

// YES if the receiver is acting as an overlay coordinator; NO (the default)
// otherwise.
@property(nonatomic, readonly) BOOL overlaying;

// The coordinator (if any) in the coordinator hierarchy (starting with
// the receiver) that is overlaying. If the receiver isn't overlaying,
// it recursively asks its children.
@property(nonatomic, readonly) BrowserCoordinator* overlayCoordinator;

// Adds |coordinator| as a child, taking ownership of it, setting the receiver's
// viewController (if any) as the child's baseViewController, and setting
// the receiver's browserState as the child's browserState.
- (void)addChildCoordinator:(BrowserCoordinator*)coordinator;

// Removes |coordinator| as a child, relinquishing ownership of it. If
// |coordinator| isn't a child of the receiver, this method does nothing.
- (void)removeChildCoordinator:(BrowserCoordinator*)coordinator;

// Methods for adding overlay coordinators.

// Returns YES if the receiver will take |overlayCoordinator| as a child.
// The default is to return YES only if the receiver has no children, if
// the receiver has a nil -overlayCoordinator, and if |overlayCoordinator|
// is not already overlaying.
- (BOOL)canAddOverlayCoordinator:(BrowserCoordinator*)overlayCoordinator;

// Adds |overlayCoordinator| as a child to the receiver, or if it cannot be
// added, recursively add it to the receiver's child. If a receiver has
// multiple children and returns YES from -canAddOverlayCoordinator:, it
// must override this method to determines how the overlay is added.
// If neither the receiver or any child can add |overlayCoordinator|, then
// nothing happens.
- (void)addOverlayCoordinator:(BrowserCoordinator*)overlayCoordinator;

// Removes the current overlay coordinator (if any) as a child from its
// parent.
- (void)removeOverlayCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_COORDINATOR_INTERNAL_H_
