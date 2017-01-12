// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#import "ios/clean/chrome/app/steps/launch_to_basic.h"

#include "ios/chrome/app/startup/provider_registration.h"
#include "ios/clean/chrome/app/application_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ProviderInitializer

- (BOOL)canRunInState:(ApplicationState*)state {
  return state.phase == APPLICATION_COLD;
}

- (void)runInState:(ApplicationState*)state {
  [ProviderRegistration registerProviders];

  state.phase = APPLICATION_BASIC;
}

@end
