// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_display_list_recording_source.h"

namespace cc {

bool FakeDisplayListRecordingSource::IsSuitableForGpuRasterization() const {
  if (force_unsuitable_for_gpu_rasterization_)
    return false;
  return DisplayListRecordingSource::IsSuitableForGpuRasterization();
}

void FakeDisplayListRecordingSource::
    SetUnsuitableForGpuRasterizationForTesting() {
  force_unsuitable_for_gpu_rasterization_ = true;
}

}  // namespace cc
