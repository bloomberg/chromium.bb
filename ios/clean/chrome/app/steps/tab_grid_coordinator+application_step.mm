// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/app/steps/tab_grid_coordinator+application_step.h"

#import "base/supports_user_data.h"
#import "ios/clean/chrome/app/application_state.h"
#import "ios/clean/chrome/browser/browser_coordinator+internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// SupportsUserData storage container for a BrowserCoordinator object, which
// keeps a strong reference to the object it is initialized with.
class RootCoordinatorContainer : public base::SupportsUserData::Data {
 public:
  explicit RootCoordinatorContainer(BrowserCoordinator* coordinator)
      : coordinator_(coordinator) {}

 private:
  // Mark this variable unused to avoid a compiler warning; the compiler
  // doesn't have visibility into the ARC-injected retain/release.
  BrowserCoordinator* __attribute__((unused)) coordinator_;
};

const char kRootCoordinatorContainerKey[] = "root_coordinator";
}  // namespace

@interface StopTabGridCoordinator : NSObject<ApplicationStep>
@end

@implementation TabGridCoordinator (ApplicationStep)

- (BOOL)canRunInState:(ApplicationState*)state {
  return [state.window isKeyWindow] && state.phase == APPLICATION_FOREGROUNDED;
}

- (void)runInState:(ApplicationState*)state {
  self.browserState = state.browserState;
  [self start];

  state.window.rootViewController = self.viewController;

  // Size the main view controller to fit the whole screen.
  [self.viewController.view setFrame:state.window.bounds];

  // Tell the application state to use this object to open URLs.
  state.URLOpener = self;

  // Show the window.
  state.window.hidden = NO;

  // Make sure this object stays alive.
  state.persistentState->SetUserData(kRootCoordinatorContainerKey,
                                     new RootCoordinatorContainer(self));

  // Add a termination step to remove this object.
  [[state terminationSteps] addObject:[[StopTabGridCoordinator alloc] init]];
}

@end

@implementation StopTabGridCoordinator

- (BOOL)canRunInState:(ApplicationState*)state {
  return state.phase = APPLICATION_TERMINATING;
}

- (void)runInState:(ApplicationState*)state {
  state.persistentState->RemoveUserData(kRootCoordinatorContainerKey);
}

@end
