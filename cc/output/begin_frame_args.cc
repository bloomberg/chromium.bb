// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/begin_frame_args.h"

namespace cc {

BeginFrameArgs::BeginFrameArgs()
  : frame_time(base::TimeTicks()),
    deadline(base::TimeTicks()),
    interval(base::TimeDelta::FromMicroseconds(-1)) {
}

BeginFrameArgs::BeginFrameArgs(base::TimeTicks frame_time,
                               base::TimeTicks deadline,
                               base::TimeDelta interval)
  : frame_time(frame_time),
    deadline(deadline),
    interval(interval)
{}

BeginFrameArgs BeginFrameArgs::Create(base::TimeTicks frame_time,
                                      base::TimeTicks deadline,
                                      base::TimeDelta interval) {
  return BeginFrameArgs(frame_time, deadline, interval);
}

BeginFrameArgs BeginFrameArgs::CreateForSynchronousCompositor() {
  // For WebView/SynchronousCompositor, we always want to draw immediately,
  // so we set the deadline to 0 and guess that the interval is 16 milliseconds.
  return BeginFrameArgs(base::TimeTicks::Now(),
                        base::TimeTicks(),
                        DefaultInterval());
}

BeginFrameArgs BeginFrameArgs::CreateForTesting() {
  base::TimeTicks now = base::TimeTicks::Now();
  return BeginFrameArgs(now,
                        now + (DefaultInterval() / 2),
                        DefaultInterval());
}

BeginFrameArgs BeginFrameArgs::CreateExpiredForTesting() {
  base::TimeTicks now = base::TimeTicks::Now();
  return BeginFrameArgs(now,
                        now - DefaultInterval(),
                        DefaultInterval());
}

base::TimeDelta BeginFrameArgs::DefaultDeadlineAdjustment() {
  // Using a large deadline adjustment will effectively revert BeginFrame
  // scheduling to the hard vsync scheduling we used to have.
  return base::TimeDelta::FromSeconds(-1);
}

base::TimeDelta BeginFrameArgs::DefaultInterval() {
  return base::TimeDelta::FromMicroseconds(16666);
}

base::TimeDelta BeginFrameArgs::DefaultRetroactiveBeginFramePeriod() {
  return base::TimeDelta::FromMicroseconds(4444);
}


}  // namespace cc
