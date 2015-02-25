// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SRT_FIELD_TRIAL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_SRT_FIELD_TRIAL_WIN_H_

namespace safe_browsing {

// Returns true if this Chrome is in a field trial group which shows the SRT
// prompt.
bool IsInSRTPromptFieldTrialGroups();

// Returns the correct SRT download URL for the current field trial.
const char* GetSRTDownloadURL();

// Returns the value of the incoming SRT seed.
std::string GetIncomingSRTSeed();

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SRT_FIELD_TRIAL_WIN_H_
