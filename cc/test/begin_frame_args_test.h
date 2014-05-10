// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_BEGIN_FRAME_ARGS_TEST_H_
#define CC_TEST_BEGIN_FRAME_ARGS_TEST_H_

#include <iosfwd>

#include "base/time/time.h"
#include "cc/output/begin_frame_args.h"

namespace cc {

// Functions for quickly creating BeginFrameArgs
BeginFrameArgs CreateBeginFrameArgsForTesting();
BeginFrameArgs CreateBeginFrameArgsForTesting(int64 frame_time,
                                              int64 deadline,
                                              int64 interval);
BeginFrameArgs CreateExpiredBeginFrameArgsForTesting();

}  // namespace cc

#endif  // CC_TEST_BEGIN_FRAME_ARGS_TEST_H_
