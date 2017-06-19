// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/surface_dependency_deadline.h"

#include "cc/surfaces/surface_dependency_tracker.h"

namespace cc {

SurfaceDependencyDeadline::SurfaceDependencyDeadline(
    BeginFrameSource* begin_frame_source)
    : begin_frame_source_(begin_frame_source) {
  DCHECK(begin_frame_source_);
}

SurfaceDependencyDeadline::~SurfaceDependencyDeadline() {
  // The deadline must be canceled before destruction.
  DCHECK(!number_of_frames_to_deadline_);
}

void SurfaceDependencyDeadline::Set(uint32_t number_of_frames_to_deadline) {
  DCHECK_GT(number_of_frames_to_deadline, 0u);
  DCHECK(!number_of_frames_to_deadline_);
  number_of_frames_to_deadline_ = number_of_frames_to_deadline;
  begin_frame_source_->AddObserver(this);
}

void SurfaceDependencyDeadline::Cancel() {
  if (!number_of_frames_to_deadline_)
    return;
  begin_frame_source_->RemoveObserver(this);
  number_of_frames_to_deadline_.reset();
}

// BeginFrameObserver implementation.
void SurfaceDependencyDeadline::OnBeginFrame(const BeginFrameArgs& args) {
  last_begin_frame_args_ = args;
  if (--(*number_of_frames_to_deadline_) > 0)
    return;

  Cancel();
  for (auto& observer : observer_list_)
    observer.OnDeadline();
}

const BeginFrameArgs& SurfaceDependencyDeadline::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void SurfaceDependencyDeadline::OnBeginFrameSourcePausedChanged(bool paused) {}

}  // namespace cc
