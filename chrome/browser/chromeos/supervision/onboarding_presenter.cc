// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/supervision/onboarding_presenter.h"

#include <utility>

#include "chromeos/constants/chromeos_features.h"

namespace chromeos {
namespace supervision {

OnboardingPresenter::OnboardingPresenter(OnboardingFlowModel* flow_model)
    : flow_model_(flow_model) {
  flow_model_->AddObserver(this);
}

OnboardingPresenter::~OnboardingPresenter() {
  flow_model_->RemoveObserver(this);
}

void OnboardingPresenter::StepStartedLoading(OnboardingFlowModel::Step step) {
  auto presentation = mojom::OnboardingPresentation::New();
  presentation->state = mojom::OnboardingPresentationState::kLoading;

  flow_model_->GetWebviewHost().SetPresentation(std::move(presentation));
}

void OnboardingPresenter::StepFinishedLoading(OnboardingFlowModel::Step step) {
  if (!base::FeatureList::IsEnabled(features::kSupervisionOnboardingScreens)) {
    flow_model_->ExitFlow(OnboardingFlowModel::ExitReason::kFeatureDisabled);
    return;
  }

  auto presentation = mojom::OnboardingPresentation::New();
  presentation->state = mojom::OnboardingPresentationState::kReady;
  presentation->can_show_next_page = true;
  switch (step) {
    case OnboardingFlowModel::Step::kStart:
      presentation->can_skip_flow = true;
      break;
    case OnboardingFlowModel::Step::kDetails:
    case OnboardingFlowModel::Step::kAllSet:
      presentation->can_show_previous_page = true;
      break;
  }

  flow_model_->GetWebviewHost().SetPresentation(std::move(presentation));
}

}  // namespace supervision
}  // namespace chromeos
