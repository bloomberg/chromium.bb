// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_APP_APPLICATION_PHASE_H_
#define IOS_CLEAN_CHROME_APP_APPLICATION_PHASE_H_

// The phases that the application moves through as it transitions between
// states.
typedef NS_ENUM(NSUInteger, ApplicationPhase) {
  // The initial state of the application performing a cold launch.
  APPLICATION_COLD,
  // The minimal initialization that must be completed before any further
  // startup can happen. |applicationDidFinishLaunching:withOptions:| must
  // bring the appication to at least this phase before returning.
  APPLICATION_BASIC,
  // The phase required for any background handling.
  APPLICATION_BACKGROUNDED,
  // The phase where the app is foregrounded but Safe Mode has been started.
  APPLICATION_SAFE_MODE,
  // The phase where the app is fully foregrounded and the regular UI has
  // started.
  APPLICATION_FOREGROUNDED,
  // The phase where the application has started to terminate.
  APPLICATION_TERMINATING
};

#endif  // IOS_CLEAN_CHROME_APP_APPLICATION_PHASE_H_
