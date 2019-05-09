// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/time_to_first_present_recorder.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/presentation_feedback.h"

namespace ash {

// static
const char TimeToFirstPresentRecorder::kMetricName[] =
    "Ash.ProcessCreationToFirstPresent";

TimeToFirstPresentRecorder::TimeToFirstPresentRecorder(aura::Window* window) {
  aura::WindowTreeHost* window_tree_host = window->GetHost();
  DCHECK(window_tree_host);
  window_tree_host->compositor()->RequestPresentationTimeForNextFrame(
      base::BindOnce(&TimeToFirstPresentRecorder::DidPresentCompositorFrame,
                     base::Unretained(this)));
}

TimeToFirstPresentRecorder::~TimeToFirstPresentRecorder() = default;

void TimeToFirstPresentRecorder::DidPresentCompositorFrame(
    const gfx::PresentationFeedback& feedback) {
  const base::TimeDelta time_to_first_present =
      feedback.timestamp - startup_metric_utils::MainEntryPointTicks();
  UMA_HISTOGRAM_TIMES(kMetricName, time_to_first_present);
  if (log_callback_)
    std::move(log_callback_).Run();
}

}  // namespace ash
