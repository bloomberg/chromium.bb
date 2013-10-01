// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_CALLBACK_H_
#define CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_CALLBACK_H_

#include "base/callback_forward.h"

// A callback invoked by the settings reset bubble when the user chooses to
// reset her preferences. The bool provided in the first argument is true when
// the user has consented to provide feedback to Google. Implementations are
// responsible for running the closure provided in the second argument when the
// reset has completed.
typedef base::Callback<void(bool, const base::Closure&)> ProfileResetCallback;

#endif  // CHROME_BROWSER_PROFILE_RESETTER_PROFILE_RESET_CALLBACK_H_
