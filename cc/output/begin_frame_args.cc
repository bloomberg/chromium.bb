// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/begin_frame_args.h"

#include "base/debug/trace_event_argument.h"
#include "ui/gfx/frame_time.h"

namespace cc {

BeginFrameArgs::BeginFrameArgs()
    : frame_time(base::TimeTicks()),
      deadline(base::TimeTicks()),
      interval(base::TimeDelta::FromMicroseconds(-1)),
      type(BeginFrameArgs::INVALID) {
}

BeginFrameArgs::BeginFrameArgs(base::TimeTicks frame_time,
                               base::TimeTicks deadline,
                               base::TimeDelta interval,
                               BeginFrameArgs::BeginFrameArgsType type)
    : frame_time(frame_time),
      deadline(deadline),
      interval(interval),
      type(type) {
}

BeginFrameArgs BeginFrameArgs::CreateTyped(
    base::TimeTicks frame_time,
    base::TimeTicks deadline,
    base::TimeDelta interval,
    BeginFrameArgs::BeginFrameArgsType type) {
  DCHECK_NE(type, BeginFrameArgs::INVALID);
  return BeginFrameArgs(frame_time, deadline, interval, type);
}

BeginFrameArgs BeginFrameArgs::Create(base::TimeTicks frame_time,
                                      base::TimeTicks deadline,
                                      base::TimeDelta interval) {
  return CreateTyped(frame_time, deadline, interval, BeginFrameArgs::NORMAL);
}

scoped_refptr<base::debug::ConvertableToTraceFormat> BeginFrameArgs::AsValue()
    const {
  scoped_refptr<base::debug::TracedValue> state =
      new base::debug::TracedValue();
  AsValueInto(state.get());
  return state;
}

void BeginFrameArgs::AsValueInto(base::debug::TracedValue* state) const {
  state->SetString("type", "BeginFrameArgs");
  switch (type) {
    case BeginFrameArgs::INVALID:
      state->SetString("subtype", "INVALID");
      break;
    case BeginFrameArgs::NORMAL:
      state->SetString("subtype", "NORMAL");
      break;
    case BeginFrameArgs::SYNCHRONOUS:
      state->SetString("subtype", "SYNCHRONOUS");
      break;
    case BeginFrameArgs::MISSED:
      state->SetString("subtype", "MISSED");
      break;
  }
  state->SetDouble("frame_time_us", frame_time.ToInternalValue());
  state->SetDouble("deadline_us", deadline.ToInternalValue());
  state->SetDouble("interval_us", interval.InMicroseconds());
}

BeginFrameArgs BeginFrameArgs::CreateForSynchronousCompositor(
    base::TimeTicks now) {
  // For WebView/SynchronousCompositor, we always want to draw immediately,
  // so we set the deadline to 0 and guess that the interval is 16 milliseconds.
  if (now.is_null())
    now = gfx::FrameTime::Now();
  return CreateTyped(
      now, base::TimeTicks(), DefaultInterval(), BeginFrameArgs::SYNCHRONOUS);
}

// This is a hard-coded deadline adjustment that assumes 60Hz, to be used in
// cases where a good estimated draw time is not known. Using 1/3 of the vsync
// as the default adjustment gives the Browser the last 1/3 of a frame to
// produce output, the Renderer Impl thread the middle 1/3 of a frame to produce
// ouput, and the Renderer Main thread the first 1/3 of a frame to produce
// output.
base::TimeDelta BeginFrameArgs::DefaultEstimatedParentDrawTime() {
  return base::TimeDelta::FromMicroseconds(16666 / 3);
}

base::TimeDelta BeginFrameArgs::DefaultInterval() {
  return base::TimeDelta::FromMicroseconds(16666);
}

base::TimeDelta BeginFrameArgs::DefaultRetroactiveBeginFramePeriod() {
  return base::TimeDelta::FromMicroseconds(4444);
}

}  // namespace cc
