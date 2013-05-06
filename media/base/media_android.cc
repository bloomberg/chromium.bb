// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <cpu-features.h>

#include "base/files/file_path.h"

namespace media {
namespace internal {

bool InitializeMediaLibraryInternal(const base::FilePath& module_dir) {
  // No real initialization is necessary but we do need to check if
  // Neon is supported because the FFT library requires Neon.
  return (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0;
}

}  // namespace internal
}  // namespace media
