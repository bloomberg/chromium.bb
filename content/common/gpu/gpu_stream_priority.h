// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_STREAM_PRIORITY_H_
#define CONTENT_COMMON_GPU_GPU_STREAM_PRIORITY_H_

namespace content {

enum class GpuStreamPriority {
  REAL_TIME,
  NORMAL,
  LOW,
  INHERIT,
  LAST = INHERIT
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_GPU_STREAM_PRIORITY_H_
