// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/surfaces/surface_dependency_deadline.h"

namespace viz {

SurfaceDependencyDeadline::SurfaceDependencyDeadline(
    SurfaceDeadlineClient* client,
    BeginFrameSource* begin_frame_source)
    : client_(client), begin_frame_source_(begin_frame_source) {
  DCHECK(client_);
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

bool SurfaceDependencyDeadline::InheritFrom(
    const SurfaceDependencyDeadline& other) {
  if (*this == other)
    return false;

  Cancel();
  last_begin_frame_args_ = other.last_begin_frame_args_;
  begin_frame_source_ = other.begin_frame_source_;
  number_of_frames_to_deadline_ = other.number_of_frames_to_deadline_;
  if (number_of_frames_to_deadline_)
    begin_frame_source_->AddObserver(this);
  return true;
}

bool SurfaceDependencyDeadline::operator==(
    const SurfaceDependencyDeadline& other) {
  return begin_frame_source_ == other.begin_frame_source_ &&
         number_of_frames_to_deadline_ == other.number_of_frames_to_deadline_;
}

// BeginFrameObserver implementation.
void SurfaceDependencyDeadline::OnBeginFrame(const BeginFrameArgs& args) {
  last_begin_frame_args_ = args;
  // OnBeginFrame might get called immediately after cancellation if some other
  // deadline triggered this deadline to be canceled.
  if (!number_of_frames_to_deadline_ || args.type == BeginFrameArgs::MISSED)
    return;

  if (--(*number_of_frames_to_deadline_) > 0)
    return;

  Cancel();

  client_->OnDeadline();
}

const BeginFrameArgs& SurfaceDependencyDeadline::LastUsedBeginFrameArgs()
    const {
  return last_begin_frame_args_;
}

void SurfaceDependencyDeadline::OnBeginFrameSourcePausedChanged(bool paused) {}

}  // namespace viz
