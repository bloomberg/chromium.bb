// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FIELD_TRIAL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FIELD_TRIAL_H_

#include <string>

#include "base/strings/string16.h"

// Summer/Fall 2013 Finch experiment strings ---------------------------------
// Only deployed to English speakers, don't need translation.

extern const char kMalwareWarningFinchTrialName[];

// Helper for getting the appropriate message for a Finch trial.
// You should only invoke this if you believe you're in the kFinchTrialName
// finch trial; if you aren't, use the default string and don't invoke this.
base::string16 AssembleMalwareFinchString(
    const std::string& trial_condition,
    const base::string16& elided_filename);

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FIELD_TRIAL_H_
