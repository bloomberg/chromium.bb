// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/steps/help_step.h"

#include "chrome/browser/chromeos/first_run/first_run_controller.h"
#include "chrome/browser/chromeos/first_run/step_names.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"
#include "ui/gfx/geometry/rect.h"

namespace {

const int kCircleRadius = 19;

}  // namespace

namespace chromeos {
namespace first_run {

HelpStep::HelpStep(FirstRunController* controller, FirstRunActor* actor)
    : Step(kHelpStep, controller, actor) {}

void HelpStep::DoShow() {
  if (!first_run_controller()->IsTrayBubbleOpened())
    first_run_controller()->OpenTrayBubble();
  gfx::Rect button_bounds = first_run_controller()->GetHelpButtonBounds();
  gfx::Point center = button_bounds.CenterPoint();
  actor()->AddRoundHole(center.x(), center.y(), kCircleRadius);
  actor()->ShowStepPointingTo(name(), center.x(), center.y(), kCircleRadius);
}

void HelpStep::DoOnAfterHide() {
  first_run_controller()->CloseTrayBubble();
}

}  // namespace first_run
}  // namespace chromeos

