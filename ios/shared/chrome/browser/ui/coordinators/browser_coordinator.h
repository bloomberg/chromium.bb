// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHARED_CHROME_BROWSER_UI_COORDINATORS_BROWSER_COORDINATOR_H_
#define IOS_SHARED_CHROME_BROWSER_UI_COORDINATORS_BROWSER_COORDINATOR_H_

#import <UIKit/UIKit.h>

class Browser;
@class CoordinatorContext;

// An object that manages a UI component via a view controller.
// This is the public interface to this class; subclasses should also import
// the Internal category header (browser_coordinator+internal.h). This header
// file declares all the methods and properties a subclass must either override,
// call, or reset.
@interface BrowserCoordinator : NSObject

// The context object for this coordinator.
@property(nonatomic, strong, readonly) CoordinatorContext* context;

// The browser object used by this coordinator and passed into any child
// coordinators added to it. This is a weak pointer, and setting this property
// doesn't transfer ownership of the browser.
@property(nonatomic, assign) Browser* browser;

// The basic lifecycle methods for coordinators are -start and -stop. These
// implementations only notify the parent coordinator when this coordinator did
// start and will stop. Child classes are expected to override and call the
// superclass method at the end of -start and at the beginning of -stop.

// Starts the user interaction managed by the receiver. Typical implementations
// will create a view controller and then use |baseViewController| to present
// it. This method needs to be called at the end of the overriding
// implementation.
- (void)start NS_REQUIRES_SUPER;

// Stops the user interaction managed by the receiver. This method needs to be
// called at the beginning of the overriding implementation.
- (void)stop NS_REQUIRES_SUPER;

@end

#endif  // IOS_SHARED_CHROME_BROWSER_UI_COORDINATORS_BROWSER_COORDINATOR_H_
