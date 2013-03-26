// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_LATENCY_INFO_H_
#define CC_DEBUG_LATENCY_INFO_H_

#include "base/time.h"

namespace cc {

struct LatencyInfo {
  int64 renderer_main_frame_number;
  int64 renderer_impl_frame_number;
  int64 browser_main_frame_number;
  int64 browser_impl_frame_number;

  base::TimeTicks swap_timestamp;

  LatencyInfo() :
      renderer_main_frame_number(0),
      renderer_impl_frame_number(0),
      browser_main_frame_number(0),
      browser_impl_frame_number(0) {}
};

}  // namespace cc

#endif  // CC_DEBUG_LATENCY_INFO_H_

