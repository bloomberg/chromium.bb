// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/load_progress_tracker.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"

namespace content {
namespace {

const int kMinimumDelayBetweenUpdatesMS = 100;

// This matches what blink's ProgrssTracker has traditionally used for a
// minimum progress value.
const double kMinimumProgress = 0.1;

}

LoadProgressTracker::LoadProgressTracker(RenderViewImpl* render_view)
    : render_view_(render_view),
      total_progress_(0.0),
      weak_factory_(this) {
}

LoadProgressTracker::~LoadProgressTracker() {
}

void LoadProgressTracker::DidStartLoading(int frame_routing_id) {
  progresses_[frame_routing_id] = kMinimumProgress;
  SendChangeLoadProgress();
}

void LoadProgressTracker::DidStopLoading(int frame_routing_id) {
  if (progresses_.find(frame_routing_id) == progresses_.end())
    return;

  // Load stopped while we were still tracking load.  Make sure we update
  // progress based on this frame's completion.
  progresses_[frame_routing_id] = 1.0;
  SendChangeLoadProgress();
  // Then we clean-up our states.
  if (total_progress_ == 1.0)
    ResetStates();
}

void LoadProgressTracker::DidChangeLoadProgress(int frame_routing_id,
                                                double progress) {
  progresses_[frame_routing_id] = progress;

  // We send the progress change to the browser immediately for the first and
  // last updates.  Also, since the message loop may be pretty busy when a page
  // is loaded, it might not execute a posted task in a timely manner so we make
  // sure to immediately send progress report if enough time has passed.
  base::TimeDelta min_delay =
      base::TimeDelta::FromMilliseconds(kMinimumDelayBetweenUpdatesMS);
  if (progress == 1.0 || last_time_progress_sent_.is_null() ||
      base::TimeTicks::Now() - last_time_progress_sent_ > min_delay) {
    // If there is a pending task to send progress, it is now obsolete.
    weak_factory_.InvalidateWeakPtrs();
    SendChangeLoadProgress();
    if (total_progress_ == 1.0)
      ResetStates();
    return;
  }

  if (weak_factory_.HasWeakPtrs())
    return;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&LoadProgressTracker::SendChangeLoadProgress,
                 weak_factory_.GetWeakPtr()),
      min_delay);
}

void LoadProgressTracker::SendChangeLoadProgress() {
  last_time_progress_sent_ = base::TimeTicks::Now();
  double progress = 0.0;
  unsigned frameCount = 0;
  ProgressMap::iterator end = progresses_.end();
  for (ProgressMap::iterator it = progresses_.begin(); it != end; ++it) {
    progress += it->second;
    frameCount++;
  }
  if (frameCount == 0)
    return;
  progress /= frameCount;
  DCHECK(progress <= 1.0);

  if (progress <= total_progress_)
    return;
  total_progress_ = progress;

  render_view_->Send(
      new ViewHostMsg_DidChangeLoadProgress(render_view_->routing_id(),
                                            progress));
}

void LoadProgressTracker::ResetStates() {
  progresses_.clear();
  total_progress_ = 0.0;
  weak_factory_.InvalidateWeakPtrs();
  last_time_progress_sent_ = base::TimeTicks();
}

}  // namespace content
