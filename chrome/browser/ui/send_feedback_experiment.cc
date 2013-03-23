// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_feedback_experiment.h"

#include "base/metrics/field_trial.h"
#include "grit/generated_resources.h"

namespace chrome {

// Constants for Send Feedback Link Location Experiment
const char kSendFeedbackLinkExperimentName[] =
    "SendFeedbackLinkLocation";
const char kSendFeedbackLinkExperimentAltText[] = "alt-text";
const char kSendFeedbackLinkExperimentAltLocation[] = "alt-location";
const char kSendFeedbackLinkExperimentAltTextAndLocation[] =
    "alt-text-and-location";

bool UseAlternateSendFeedbackText() {
  std::string trial_name =
      base::FieldTrialList::FindFullName(kSendFeedbackLinkExperimentName);
  return trial_name == kSendFeedbackLinkExperimentAltText ||
         trial_name == kSendFeedbackLinkExperimentAltTextAndLocation;
}

bool UseAlternateSendFeedbackLocation() {
  std::string trial_name =
      base::FieldTrialList::FindFullName(kSendFeedbackLinkExperimentName);
  return trial_name == kSendFeedbackLinkExperimentAltLocation ||
         trial_name == kSendFeedbackLinkExperimentAltTextAndLocation;
}

int GetSendFeedbackMenuLabelID() {
  return UseAlternateSendFeedbackText() ? IDS_FEEDBACK_ALT : IDS_FEEDBACK;
}

}  // namespace chrome
