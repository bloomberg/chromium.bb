// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/step.h"

#include "ash/first_run/first_run_helper.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
namespace first_run {

Step::Step(const std::string& name,
           ash::FirstRunHelper* shell_helper,
           FirstRunActor* actor)
    : name_(name),
      shell_helper_(shell_helper),
      actor_(actor) {
}

Step::~Step() {}

void Step::OnBeforeHide() {
  actor()->RemoveBackgroundHoles();
}

gfx::Size Step::GetOverlaySize() const {
  return shell_helper()->GetOverlayWidget()->GetWindowBoundsInScreen().size();
}

}  // namespace first_run
}  // namespace chromeos

