// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_RENDERING_STATS_H_
#define CONTENT_COMMON_GPU_GPU_RENDERING_STATS_H_

#include "base/time.h"
#include "cc/rendering_stats.h"
#include "content/common/content_export.h"

namespace content {

struct CONTENT_EXPORT GpuRenderingStats {
  GpuRenderingStats();
  ~GpuRenderingStats();

  int global_texture_upload_count;
  base::TimeDelta global_total_texture_upload_time;
  int texture_upload_count;
  base::TimeDelta total_texture_upload_time;
  base::TimeDelta global_total_processing_commands_time;
  base::TimeDelta total_processing_commands_time;
  // Note: when adding new members, please remember to update enumerateFields
  // in gpu_rendering_stats.cc.

  // Outputs the fields in this structure to the provided enumerator.
  void EnumerateFields(cc::RenderingStats::Enumerator* enumerator) const;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RENDERING_STATS_H_
