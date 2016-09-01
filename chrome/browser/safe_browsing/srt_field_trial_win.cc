// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_field_trial_win.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "components/variations/variations_associated_data.h"

namespace {

// Field trial strings.
const char kSRTPromptTrial[] = "SRTPromptFieldTrial";
const char kSRTCanaryGroup[] = "SRTCanary";
const char kSRTPromptOffGroup[] = "Off";
const char kSRTPromptSeedParam[] = "Seed";

const char kSRTElevationTrial[] = "SRTElevation";
const char kSRTElevationAsNeededGroup[] = "AsNeeded";

const char kSRTReporterTrial[] = "srt_reporter";
const char kSRTReporterOffGroup[] = "Off";

// The download links of the Software Removal Tool.
const char kMainSRTDownloadURL[] =
    "https://dl.google.com/dl"
    "/softwareremovaltool/win/chrome_cleanup_tool.exe?chrome-prompt=1";
const char kCanarySRTDownloadURL[] =
    "https://dl.google.com/dl"
    "/softwareremovaltool/win/c/chrome_cleanup_tool.exe?chrome-prompt=1";

}  // namespace

namespace safe_browsing {

bool IsInSRTPromptFieldTrialGroups() {
  return !base::StartsWith(base::FieldTrialList::FindFullName(kSRTPromptTrial),
                           kSRTPromptOffGroup, base::CompareCase::SENSITIVE);
}

bool SRTPromptNeedsElevationIcon() {
  return !base::StartsWith(
      base::FieldTrialList::FindFullName(kSRTElevationTrial),
      kSRTElevationAsNeededGroup, base::CompareCase::SENSITIVE);
}

bool IsSwReporterEnabled() {
  return !base::StartsWith(
      base::FieldTrialList::FindFullName(kSRTReporterTrial),
      kSRTReporterOffGroup, base::CompareCase::SENSITIVE);
}

const char* GetSRTDownloadURL() {
  if (base::StartsWith(base::FieldTrialList::FindFullName(kSRTPromptTrial),
                       kSRTCanaryGroup, base::CompareCase::SENSITIVE)) {
    return kCanarySRTDownloadURL;
  }
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
