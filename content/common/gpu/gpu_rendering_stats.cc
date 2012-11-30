// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_rendering_stats.h"

namespace content {

GpuRenderingStats::GpuRenderingStats()
    : global_texture_upload_count(0),
      texture_upload_count(0) {
}

GpuRenderingStats::~GpuRenderingStats() {
}

void GpuRenderingStats::EnumerateFields(
    cc::RenderingStats::Enumerator* enumerator) const {
  enumerator->AddInt("globalTextureUploadCount", global_texture_upload_count);
  enumerator->AddTimeDeltaInSecondsF("globalTotalTextureUploadTimeInSeconds",
                                     global_total_texture_upload_time);
  enumerator->AddInt("textureUploadCount", texture_upload_count);
  enumerator->AddTimeDeltaInSecondsF("totalTextureUploadTimeInSeconds",
                           total_texture_upload_time);
  enumerator->AddTimeDeltaInSecondsF(
      "globalTotalProcessingCommandsTimeInSeconds",
      global_total_processing_commands_time);
  enumerator->AddTimeDeltaInSecondsF("totalProcessingCommandsTimeInSeconds",
                                     total_processing_commands_time);
}

}  // namespace content
