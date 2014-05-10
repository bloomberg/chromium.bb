// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/begin_frame_args_test.h"

#include "base/time/time.h"
#include "cc/output/begin_frame_args.h"
#include "ui/gfx/frame_time.h"

namespace cc {

BeginFrameArgs CreateBeginFrameArgsForTesting() {
  base::TimeTicks now = gfx::FrameTime::Now();
  return BeginFrameArgs::Create(now,
                                now + (BeginFrameArgs::DefaultInterval() / 2),
                                BeginFrameArgs::DefaultInterval());
}

BeginFrameArgs CreateBeginFrameArgsForTesting(int64 frame_time,
                                              int64 deadline,
                                              int64 interval) {
  return BeginFrameArgs::Create(base::TimeTicks::FromInternalValue(frame_time),
                                base::TimeTicks::FromInternalValue(deadline),
                                base::TimeDelta::FromInternalValue(interval));
}

BeginFrameArgs CreateExpiredBeginFrameArgsForTesting() {
  base::TimeTicks now = gfx::FrameTime::Now();
  return BeginFrameArgs::Create(now,
                                now - BeginFrameArgs::DefaultInterval(),
                                BeginFrameArgs::DefaultInterval());
}

}  // namespace cc
