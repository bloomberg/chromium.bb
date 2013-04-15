// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"

#include <cpu-features.h>

#include "base/logging.h"

namespace media {

// Caches the result of the check for NEON support.
static bool g_media_library_is_initialized = false;

bool InitializeMediaLibrary(const base::FilePath& module_dir) {
  // No real initialization is necessary but we do need to check if
  // Neon is supported because the FFT library requires Neon.
  g_media_library_is_initialized = (android_getCpuFeatures() &
                                    ANDROID_CPU_ARM_FEATURE_NEON) != 0;
  return g_media_library_is_initialized;
}

void InitializeMediaLibraryForTesting() {
}

bool IsMediaLibraryInitialized() {
  return g_media_library_is_initialized;
}

}  // namespace media
