// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SRT_FIELD_TRIAL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_SRT_FIELD_TRIAL_WIN_H_

#include <string>

namespace safe_browsing {

// Enum values for the SRTPrompt histogram. Don't change order, always add
// to the end, before SRT_PROMPT_MAX, of course.
enum SRTPromptHistogramValue {
  SRT_PROMPT_SHOWN = 0,
  SRT_PROMPT_ACCEPTED = 1,
  SRT_PROMPT_DENIED = 2,
  SRT_PROMPT_FALLBACK = 3,
  SRT_PROMPT_DOWNLOAD_UNAVAILABLE = 4,

  SRT_PROMPT_MAX,
};

// Returns true if this Chrome is in a field trial group which shows the SRT
// prompt.
bool IsInSRTPromptFieldTrialGroups();

// Returns true if this Chrome is in a field trial group which doesn't need an
// elevation icon, i.e., the SRT won't ask for elevation on startup.
bool SRTPromptNeedsElevationIcon();

// Returns the correct SRT download URL for the current field trial.
const char* GetSRTDownloadURL();

// Returns the value of the incoming SRT seed.
std::string GetIncomingSRTSeed();

// Records a value for the SRT Prompt Histogram.
void RecordSRTPromptHistogram(SRTPromptHistogramValue value);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SRT_FIELD_TRIAL_WIN_H_
