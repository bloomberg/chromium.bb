// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_BEGIN_FRAME_H_
#define CC_OUTPUT_BEGIN_FRAME_H_

#include "base/time.h"
#include "cc/base/cc_export.h"

namespace cc {

struct CC_EXPORT BeginFrameArgs {
  // Creates an invalid set of values.
  BeginFrameArgs();

  // You should be able to find all instances where a BeginFrame has been
  // created by searching for "BeginFrameArgs::Create".
  static BeginFrameArgs Create(base::TimeTicks frame_time,
                               base::TimeTicks deadline,
                               base::TimeDelta interval);
  static BeginFrameArgs CreateForSynchronousCompositor();
  static BeginFrameArgs CreateForTesting();

  // This is the default delta that will be used to adjust the deadline when
  // proper draw-time estimations are not yet available.
  static base::TimeDelta DefaultDeadlineAdjustment();

  // This is the default interval to use to avoid sprinkling the code with
  // magic numbers.
  static base::TimeDelta DefaultInterval();

  bool IsInvalid() const {
    return interval < base::TimeDelta();
  }

  base::TimeTicks frame_time;
  base::TimeTicks deadline;
  base::TimeDelta interval;

 private:
  BeginFrameArgs(base::TimeTicks frame_time,
                 base::TimeTicks deadline,
                 base::TimeDelta interval);
};

}  // namespace cc

#endif  // CC_OUTPUT_BEGIN_FRAME_H_
