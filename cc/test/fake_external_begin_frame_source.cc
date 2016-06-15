// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_external_begin_frame_source.h"

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "cc/test/begin_frame_args_test.h"

namespace cc {

FakeExternalBeginFrameSource::FakeExternalBeginFrameSource(
    double refresh_rate,
    bool tick_automatically)
    : tick_automatically_(tick_automatically),
      milliseconds_per_frame_(1000.0 / refresh_rate),
      weak_ptr_factory_(this) {
  DetachFromThread();
}

FakeExternalBeginFrameSource::~FakeExternalBeginFrameSource() {
  DCHECK(CalledOnValidThread());
}

void FakeExternalBeginFrameSource::SetPaused(bool paused) {
  if (paused != paused_) {
    paused_ = paused;
    std::set<BeginFrameObserver*> observers(observers_);
    for (auto* obs : observers)
      obs->OnBeginFrameSourcePausedChanged(paused_);
  }
}

void FakeExternalBeginFrameSource::AddObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) == observers_.end());

  bool observers_was_empty = observers_.empty();
  observers_.insert(obs);
  obs->OnBeginFrameSourcePausedChanged(paused_);
  if (observers_was_empty && tick_automatically_)
    PostTestOnBeginFrame();
  if (client_)
    client_->OnAddObserver(obs);
}

void FakeExternalBeginFrameSource::RemoveObserver(BeginFrameObserver* obs) {
  DCHECK(obs);
  DCHECK(observers_.find(obs) != observers_.end());

  observers_.erase(obs);
  if (observers_.empty())
    begin_frame_task_.Cancel();
  if (client_)
    client_->OnRemoveObserver(obs);
}

void FakeExternalBeginFrameSource::TestOnBeginFrame(
    const BeginFrameArgs& args) {
  DCHECK(CalledOnValidThread());
  std::set<BeginFrameObserver*> observers(observers_);
  for (auto* obs : observers)
    obs->OnBeginFrame(args);
  if (tick_automatically_)
    PostTestOnBeginFrame();
}

void FakeExternalBeginFrameSource::PostTestOnBeginFrame() {
  begin_frame_task_.Reset(
      base::Bind(&FakeExternalBeginFrameSource::TestOnBeginFrame,
                 weak_ptr_factory_.GetWeakPtr()));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(begin_frame_task_.callback(),
                 CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE)),
      base::TimeDelta::FromMilliseconds(milliseconds_per_frame_));
}

}  // namespace cc
