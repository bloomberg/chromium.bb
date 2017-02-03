// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_BROWSER_COORDINATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_BROWSER_COORDINATOR_H_

#import <UIKit/UIKit.h>

@class CoordinatorContext;
namespace ios {
class ChromeBrowserState;
}

// An object that manages a UI component via a view controller.
// This is the public interface to this class; subclasses should also import
// the Internal category header (browser_coordinator+internal.h). This header
// file declares all the methods and properties a subclass must either override,
// call, or reset.
@interface BrowserCoordinator : NSObject

// The context object for this coordinator.
@property(nonatomic, strong, readonly) CoordinatorContext* context;

// The browser state used by this coordinator and passed into any child
// coordinators added to it. This is a weak pointer, and setting this property
// doesn't transfer ownership of the browser state.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

// The basic lifecycle methods for coordinators are -start and -stop. These
// are blank template methods; child classes are expected to implement them and
// do not need to invoke the superclass methods.
// Starts the user interaction managed by the receiver. Typical implementations
// will create a view controller and then use |baseViewController| to present
// it.
- (void)start;

// Stops the user interaction managed by the receiver.
- (void)stop;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_BROWSER_COORDINATOR_H_
