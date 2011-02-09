// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/load_progress_tracker.h"

#include "base/message_loop.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_view.h"

namespace {

const int kMinimumDelayBetweenUpdatesMS = 100;

}

LoadProgressTracker::LoadProgressTracker(RenderView* render_view)
    : render_view_(render_view),
      tracked_frame_(NULL),
      progress_(0.0),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

LoadProgressTracker::~LoadProgressTracker() {
}

void LoadProgressTracker::DidStopLoading() {
  if (!tracked_frame_)
    return;

  // Load stoped while we were still tracking load.  Make sure we notify the
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
  if (progress == 1.0 || last_time_progress_sent_.is_null() ||
      (base::TimeTicks::Now() -  last_time_progress_sent_).InMilliseconds() >
          kMinimumDelayBetweenUpdatesMS) {
    // If there is a pending task to send progress, it is now obsolete.
    method_factory_.RevokeAll();
    SendChangeLoadProgress();
    if (progress == 1.0)
      ResetStates();
    return;
  }

  if (!method_factory_.empty())
    return;

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(
          &LoadProgressTracker::SendChangeLoadProgress),
      kMinimumDelayBetweenUpdatesMS);
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
  method_factory_.RevokeAll();
  last_time_progress_sent_ = base::TimeTicks();
}
