// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRIALS_HTTP_THROTTLING_TRIAL_H_
#define CHROME_BROWSER_TRIALS_HTTP_THROTTLING_TRIAL_H_
#pragma once

class PrefService;

// Creates the trial.
void CreateHttpThrottlingTrial(PrefService* prefs);

#endif  // CHROME_BROWSER_TRIALS_HTTP_THROTTLING_TRIAL_H_
