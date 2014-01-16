// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/steps/help_step.h"

#include "ash/first_run/first_run_helper.h"
#include "chrome/browser/chromeos/first_run/step_names.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"
#include "ui/gfx/rect.h"

namespace {

const int kCircleRadius = 19;

}  // namespace

namespace chromeos {
namespace first_run {

HelpStep::HelpStep(ash::FirstRunHelper* shell_helper, FirstRunActor* actor)
    : Step(kHelpStep, shell_helper, actor) {
}

void HelpStep::DoShow() {
  if (!shell_helper()->IsTrayBubbleOpened())
    shell_helper()->OpenTrayBubble();
  gfx::Rect button_bounds = shell_helper()->GetHelpButtonBounds();
  gfx::Point center = button_bounds.CenterPoint();
  actor()->AddRoundHole(center.x(), center.y(), kCircleRadius);
  actor()->ShowStepPointingTo(name(), center.x(), center.y(), kCircleRadius);
}

void HelpStep::DoOnAfterHide() {
  shell_helper()->CloseTrayBubble();
}

}  // namespace first_run
}  // namespace chromeos

