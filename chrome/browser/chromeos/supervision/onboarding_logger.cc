// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_logger.h"

#include "base/logging.h"

namespace chromeos {
namespace supervision {
namespace {

const char* StepDescription(OnboardingFlowModel::Step step) {
  switch (step) {
    case OnboardingFlowModel::Step::kStart:
      return "Start";
    case OnboardingFlowModel::Step::kDetails:
      return "Details";
    case OnboardingFlowModel::Step::kAllSet:
      return "AllSet";
  }
}

const char* ExitReasonDescription(OnboardingFlowModel::ExitReason reason) {
  switch (reason) {
    case OnboardingFlowModel::ExitReason::kUserReachedEnd:
      return "User reached the end of the flow successfuly.";
    case OnboardingFlowModel::ExitReason::kFlowSkipped:
      return "User chose to skip the flow.";
    case OnboardingFlowModel::ExitReason::kAuthError:
      return "Found an error getting an access token.";
    case OnboardingFlowModel::ExitReason::kWebviewNetworkError:
      return "Webview found a network error.";
    case OnboardingFlowModel::ExitReason::kUserNotEligible:
      return "User is not eligible to go through the flow.";
    case OnboardingFlowModel::ExitReason::kFeatureDisabled:
      return "Feature is disabled, we can't present flow pages.";
  }
}

bool ExitedDueToError(OnboardingFlowModel::ExitReason reason) {
  switch (reason) {
    case OnboardingFlowModel::ExitReason::kUserReachedEnd:
    case OnboardingFlowModel::ExitReason::kFlowSkipped:
    case OnboardingFlowModel::ExitReason::kUserNotEligible:
    case OnboardingFlowModel::ExitReason::kFeatureDisabled:
      return false;
    case OnboardingFlowModel::ExitReason::kAuthError:
    case OnboardingFlowModel::ExitReason::kWebviewNetworkError:
      return true;
  }
}

}  // namespace

OnboardingLogger::OnboardingLogger(OnboardingFlowModel* flow_model)
    : flow_model_(flow_model) {
  flow_model_->AddObserver(this);
}

OnboardingLogger::~OnboardingLogger() {
  flow_model_->RemoveObserver(this);
}

void OnboardingLogger::StepStartedLoading(OnboardingFlowModel::Step step) {
  DVLOG(1) << "Supervision Onboarding started loading step: "
           << StepDescription(step);
}

void OnboardingLogger::StepFinishedLoading(OnboardingFlowModel::Step step) {
  DVLOG(1) << "Supervision Onboarding successfuly loaded step: "
           << StepDescription(step);
}

void OnboardingLogger::WillExitFlow(OnboardingFlowModel::Step step,
                                    OnboardingFlowModel::ExitReason reason) {
  if (ExitedDueToError(reason)) {
    LOG(ERROR)
        << "Supervision Onboarding is exiting because it found an error: "
        << ExitReasonDescription(reason);
    return;
  }

  DVLOG(1) << "Supervision Onboarding exiting. "
           << ExitReasonDescription(reason);
}

}  // namespace supervision
}  // namespace chromeos
