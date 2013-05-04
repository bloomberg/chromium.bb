// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/load_progress_tracker.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_view_impl.h"

namespace content {
namespace {

const int kMinimumDelayBetweenUpdatesMS = 100;

}

LoadProgressTracker::LoadProgressTracker(RenderViewImpl* render_view)
    : render_view_(render_view),
      tracked_frame_(NULL),
      progress_(0.0),
      weak_factory_(this) {
}

LoadProgressTracker::~LoadProgressTracker() {
}

void LoadProgressTracker::DidStopLoading() {
  if (!tracked_frame_)
    return;

  // Load stopped while we were still tracking load.  Make sure we notify the
  // browser that load is complete.
  progress_ = 1.0;
  SendChangeLoadProgress();
  // Then we clean-up our states.
  ResetStates();
}

void LoadProgressTracker::DidChangeLoadProgress(WebKit::WebFrame* frame,
                                                double progress) {
  if (tracked_frame_ && frame != tracked_frame_)
    return;

  if (!tracked_frame_)
    tracked_frame_ = frame;

  progress_ = progress;

  // We send the progress change to the browser immediately for the first and
  // last updates.  Also, since the message loop may be pretty busy when a page
  // is loaded, it might not execute a posted task in a timely manner so we make
  // sure to immediately send progress report if enough time has passed.
  base::TimeDelta min_delay =
      base::TimeDelta::FromMilliseconds(kMinimumDelayBetweenUpdatesMS);
  if (progress == 1.0 || last_time_progress_sent_.is_null() ||
      base::TimeTicks::Now() - last_time_progress_sent_ >
      min_delay) {
    // If there is a pending task to send progress, it is now obsolete.
    weak_factory_.InvalidateWeakPtrs();
    SendChangeLoadProgress();
    if (progress == 1.0)
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
  render_view_->Send(
      new ViewHostMsg_DidChangeLoadProgress(render_view_->routing_id(),
                                            progress_));
}

void LoadProgressTracker::ResetStates() {
  tracked_frame_ = NULL;
  progress_ = 0.0;
  weak_factory_.InvalidateWeakPtrs();
  last_time_progress_sent_ = base::TimeTicks();
}

}  // namespace content
