// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/first_run/steps/tray_step.h"

#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm_shell.h"
#include "ash/first_run/first_run_helper.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/chromeos/first_run/step_names.h"
#include "chrome/browser/ui/webui/chromeos/first_run/first_run_actor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace chromeos {
namespace first_run {

TrayStep::TrayStep(ash::FirstRunHelper* shell_helper, FirstRunActor* actor)
    : Step(kTrayStep, shell_helper, actor) {
}

void TrayStep::DoShow() {
  if (!shell_helper()->IsTrayBubbleOpened())
    shell_helper()->OpenTrayBubble();
  gfx::Rect bounds = shell_helper()->GetTrayBubbleBounds();
  actor()->AddRectangularHole(bounds.x(), bounds.y(), bounds.width(),
      bounds.height());
  FirstRunActor::StepPosition position;
  position.SetTop(bounds.y());
  ash::ShelfAlignment alignment =
      ash::WmShelf::ForWindow(ash::WmShell::Get()->GetPrimaryRootWindow())
          ->alignment();
  if ((!base::i18n::IsRTL() && alignment != ash::SHELF_ALIGNMENT_LEFT) ||
      alignment == ash::SHELF_ALIGNMENT_RIGHT)
    position.SetRight(GetOverlaySize().width() - bounds.x());
  else
    position.SetLeft(bounds.right());
  actor()->ShowStepPositioned(name(), position);
}

}  // namespace first_run
}  // namespace chromeos

