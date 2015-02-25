// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"
#include "components/variations/variations_associated_data.h"

namespace {

// Field trial strings.
const char kSRTCanaryGroupName[] = "SRTCanary";
const char kSRTPromptTrialName[] = "SRTPromptFieldTrial";
const char kSRTPromptOnGroup[] = "On";
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
  return base::FieldTrialList::FindFullName(kSRTPromptTrialName) ==
         kSRTPromptOnGroup;
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

}  // namespace safe_browsing
