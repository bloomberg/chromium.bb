// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_field_trial_win.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "components/variations/variations_associated_data.h"

namespace {

// Field trial strings.
const char kSRTCanaryGroupName[] = "SRTCanary";
const char kSRTPromptTrialName[] = "SRTPromptFieldTrial";
const char kSRTPromptOnGroup[] = "On";
const char kSRTPromptDefaultGroup[] = "Default";
const char kSRTPromptSeedParamName[] = "Seed";

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
  return base::FieldTrialList::FindFullName(kSRTPromptTrialName) !=
         kSRTPromptDefaultGroup;
}

const char* GetSRTDownloadURL() {
  if (base::FieldTrialList::FindFullName(kSRTPromptTrialName) ==
      kSRTCanaryGroupName)
    return kCanarySRTDownloadURL;
  return kMainSRTDownloadURL;
}

std::string GetIncomingSRTSeed() {
  return variations::GetVariationParamValue(kSRTPromptTrialName,
                                            kSRTPromptSeedParamName);
}

void RecordSRTPromptHistogram(SRTPromptHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.PromptUsage", value,
                            SRT_PROMPT_MAX);
}

}  // namespace safe_browsing
