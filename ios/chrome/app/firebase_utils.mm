// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/firebase_utils.h"

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/app/firebase_buildflags.h"
#if BUILDFLAG(FIREBASE_ENABLED)
#import "ios/third_party/firebase/Analytics/FirebaseCore.framework/Headers/FIRApp.h"
#endif  // BUILDFLAG(FIREBASE_ENABLED)

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kFirebaseConfiguredHistogramName[] =
    "FirstRun.IOSFirebaseConfigured";

void InitializeFirebase(bool is_first_run) {
#if BUILDFLAG(FIREBASE_ENABLED)
  // TODO(crbug.com/848115): Continue to initialize Firebase until either:
  //   - first_open event has been uploaded, or
  //   - ad click conversion attribution period is over.
  [FIRApp configure];
  FirebaseConfiguredState enabled_state =
      is_first_run ? FirebaseConfiguredState::kEnabledFirstRun
                   : FirebaseConfiguredState::kEnabledNotFirstRun;
  UMA_HISTOGRAM_ENUMERATION(kFirebaseConfiguredHistogramName, enabled_state);
#else
  // FIRApp class should not exist if Firebase is not enabled.
  DCHECK_EQ(nil, NSClassFromString(@"FIRApp"));
  UMA_HISTOGRAM_ENUMERATION(kFirebaseConfiguredHistogramName,
                            FirebaseConfiguredState::kDisabled);
#endif  // BUILDFLAG(FIREBASE_ENABLED)
}
