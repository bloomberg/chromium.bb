// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_field_trial_win.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "components/variations/variations_associated_data.h"

namespace {

// Field trial strings.
const char kSRTPromptTrial[] = "SRTPromptFieldTrial";
const char kSRTCanaryGroup[] = "SRTCanary";
const char kSRTPromptOffGroup[] = "Off";
const char kSRTPromptSeedParam[] = "Seed";

const char kSRTElevationTrial[] = "SRTElevation";
const char kSRTElevationAsNeededGroup[] = "AsNeeded";

// The download links of the Software Removal Tool.
const char kMainSRTDownloadURL[] =
    "http://dl.google.com/dl"
    "/softwareremovaltool/win/software_removal_tool.exe?chrome-prompt=1";
const char kCanarySRTDownloadURL[] =
    "http://dl.google.com/dl"
    "/softwareremovaltool/win/c/software_removal_tool.exe?chrome-prompt=1";

}  // namespace

namespace safe_browsing {

bool IsInSRTPromptFieldTrialGroups() {
  return base::FieldTrialList::FindFullName(kSRTPromptTrial) !=
         kSRTPromptOffGroup;
}

bool SRTPromptNeedsElevationIcon() {
  return base::FieldTrialList::FindFullName(kSRTElevationTrial) !=
         kSRTElevationAsNeededGroup;
}

const char* GetSRTDownloadURL() {
  if (base::FieldTrialList::FindFullName(kSRTPromptTrial) == kSRTCanaryGroup)
    return kCanarySRTDownloadURL;
  return kMainSRTDownloadURL;
}

std::string GetIncomingSRTSeed() {
  return variations::GetVariationParamValue(kSRTPromptTrial,
                                            kSRTPromptSeedParam);
}

void RecordSRTPromptHistogram(SRTPromptHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.PromptUsage", value,
                            SRT_PROMPT_MAX);
}

}  // namespace safe_browsing
