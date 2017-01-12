// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/app/application_state.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#import "ios/clean/chrome/app/application_step.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Specialization of base::SupportsUserData for use in this class.
class PersistentApplicationState : public base::SupportsUserData {
 public:
  ~PersistentApplicationState() override {}
};

}  // namespace

@interface ApplicationState ()
@property(nonatomic, readwrite, copy) NSDictionary* launchOptions;
@end

@implementation ApplicationState {
  std::unique_ptr<PersistentApplicationState> _persistentState;
}

@synthesize application = _application;
@synthesize browserState = _browserState;
@synthesize URLOpener = _URLOpener;
@synthesize phase = _phase;
@synthesize window = _window;
@synthesize launchSteps = _launchSteps;
@synthesize terminationSteps = _terminationSteps;
@synthesize backgroundSteps = _backgroundSteps;
@synthesize foregroundSteps = _foregroundSteps;
@synthesize launchOptions = _launchOptions;

#pragma mark - Object lifecycle

- (instancetype)init {
  if ((self = [super init])) {
    _phase = APPLICATION_COLD;
    _launchSteps = [ApplicationStepArray array];
    _terminationSteps = [ApplicationStepArray array];
    _backgroundSteps = [ApplicationStepArray array];
    _foregroundSteps = [ApplicationStepArray array];
    _persistentState = base::MakeUnique<PersistentApplicationState>();
  }
  return self;
}

#pragma mark - Public API

- (base::SupportsUserData*)persistentState {
  return _persistentState.get();
}

- (void)launchWithOptions:(NSDictionary*)launchOptions {
  self.launchOptions = launchOptions;
  [self continueLaunch];
}

- (void)continueLaunch {
  [self runSteps:self.launchSteps];
}

- (void)terminate {
  CHECK(self.phase != APPLICATION_TERMINATING);
  self.phase = APPLICATION_TERMINATING;
  [self runSteps:self.terminationSteps];
  CHECK(self.terminationSteps.count == 0);
}

- (void)background {
  [self runSteps:self.backgroundSteps];
}

- (void)foreground {
  [self runSteps:self.foregroundSteps];
}

#pragma mark - Running steps

// While the first step in |steps| can run in |self|, pop it, run it, and
// release ownership of it.
- (void)runSteps:(ApplicationStepArray*)steps {
  while ([steps.firstObject canRunInState:self]) {
    id<ApplicationStep> nextStep = steps.firstObject;
    [steps removeObject:nextStep];
    // |nextStep| should not be in |steps| when -runInState is called.
    // (Some steps may re-insert themselves into |steps|, for example).
    DCHECK(![steps containsObject:nextStep]);
    [nextStep runInState:self];
  }
}

@end
