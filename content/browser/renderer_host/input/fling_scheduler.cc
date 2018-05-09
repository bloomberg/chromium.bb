// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/fling_scheduler.h"

#include "build/build_config.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "ui/compositor/compositor.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

namespace content {

FlingScheduler::FlingScheduler(RenderWidgetHostImpl* host) : host_(host) {
  DCHECK(host);
}
FlingScheduler::~FlingScheduler() = default;

void FlingScheduler::ScheduleFlingProgress(
    base::WeakPtr<FlingController> fling_controller) {
  DCHECK(fling_controller);
  fling_controller_ = fling_controller;
  ui::Compositor* compositor = GetCompositor();

  if (compositor) {
    // |fling_controller->SetCompositor(compositor)| adds the fling_controller
    // as an animation observer to this compositor when fling_controller has
    // called |ScheduleFlingProgress| for the first time.
    if (!is_observer_added_) {
      fling_controller->SetCompositor(compositor);
      is_observer_added_ = true;
    }
  } else {  // No compositor is available.
    host_->SetNeedsBeginFrameForFlingProgress();
  }
}
void FlingScheduler::DidStopFlingingOnBrowser(
    base::WeakPtr<FlingController> fling_controller) {
  DCHECK(fling_controller);
  if (GetCompositor()) {
    // |fling_controller->SetCompositor(nullptr)| removes the fling_controller
    // as an animation observer from its current compositor when flinging has
    // stopped.
    fling_controller->SetCompositor(nullptr);
    is_observer_added_ = false;
  }
  fling_controller_ = nullptr;
  host_->DidStopFlinging();
}

void FlingScheduler::ProgressFlingOnBeginFrameIfneeded(
    base::TimeTicks current_time) {
  // No fling is active.
  if (!fling_controller_)
    return;

  // FlingProgress will be called within FlingController::OnAnimationStep.
  if (is_observer_added_)
    return;

  fling_controller_->ProgressFling(current_time);
}

ui::Compositor* FlingScheduler::GetCompositor() {
#if defined(USE_AURA)
  if (host_->GetView() && host_->GetView()->GetNativeView() &&
      host_->GetView()->GetNativeView()->GetHost() &&
      host_->GetView()->GetNativeView()->GetHost()->compositor()) {
    return host_->GetView()->GetNativeView()->GetHost()->compositor();
  }
#endif

  return nullptr;
}

}  // namespace content
