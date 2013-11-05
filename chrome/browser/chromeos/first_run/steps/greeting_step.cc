// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/steps/greeting_step.h"

#include "ash/first_run/first_run_helper.h"
#include "chrome/browser/chromeos/first_run/step_names.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"
#include "ui/gfx/rect.h"

namespace chromeos {
namespace first_run {

GreetingStep::GreetingStep(ash::FirstRunHelper* shell_helper,
                           FirstRunActor* actor)
    : Step(kGreetingStep, shell_helper, actor) {
}

void GreetingStep::Show() {
  actor()->ShowStepPositioned(name(), FirstRunActor::StepPosition());
}

void GreetingStep::OnBeforeHide() {
  actor()->SetBackgroundVisible(true);
}

}  // namespace first_run
}  // namespace chromeos

