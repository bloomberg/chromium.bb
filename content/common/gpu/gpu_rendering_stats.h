// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_GPU_RENDERING_STATS_H_
#define CONTENT_COMMON_GPU_GPU_RENDERING_STATS_H_

#include "base/time.h"
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

  // In conjunction with enumerateFields, this allows the embedder to
  // enumerate the values in this structure without
  // having to embed references to its specific member variables. This
  // simplifies the addition of new fields to this type.
  class Enumerator {
   public:
    virtual void addInt(const char* name, int value) = 0;
    virtual void addTimeDelta(const char* name, base::TimeDelta value) = 0;
   protected:
    virtual ~Enumerator() { }
  };

  // Outputs the fields in this structure to the provided enumerator.
  void enumerateFields(Enumerator* enumerator) const {
    enumerator->addInt("globalTextureUploadCount", global_texture_upload_count);
    enumerator->addTimeDelta("totalTextureUploadTime",
                             total_texture_upload_time);
    enumerator->addInt("textureUploadCount", texture_upload_count);
    enumerator->addTimeDelta("globalTotalProcessingCommandsTime",
                             global_total_processing_commands_time);
    enumerator->addTimeDelta("totalProcessingCommandsTime",
                             total_processing_commands_time);
  }
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_RENDERING_STATS_H_
