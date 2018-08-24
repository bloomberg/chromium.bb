// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_FIREBASE_UTILS_H_
#define IOS_CHROME_APP_FIREBASE_UTILS_H_

// Name of histogram to log whether Firebase SDK is initialized.
extern const char kFirebaseConfiguredHistogramName[];

// Firebase SDK may not be initialized, or initialized during First Run or not
// during First Run.
enum class FirebaseConfiguredState {
  kDisabled = 0,
  kEnabledFirstRun,
  kEnabledNotFirstRun,
  kMaxValue = kEnabledNotFirstRun,
};

// Initializes Firebase SDK if configured and necessary.
void InitializeFirebase(bool is_first_run);

#endif  // IOS_CHROME_APP_FIREBASE_UTILS_H_
