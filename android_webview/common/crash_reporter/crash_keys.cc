// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/crash_reporter/crash_keys.h"

#include "base/debug/crash_logging.h"
#include "components/crash/core/common/crash_keys.h"

using namespace crash_keys;

namespace android_webview {
namespace crash_keys {

const char kGPUDriverVersion[] = "gpu-driver";
const char kGPUPixelShaderVersion[] = "gpu-psver";
const char kGPUVertexShaderVersion[] = "gpu-vsver";
const char kGPUVendor[] = "gpu-gl-vendor";
const char kGPURenderer[] = "gpu-gl-renderer";

size_t RegisterWebViewCrashKeys() {
  base::debug::CrashKey fixed_keys[] = {
    { kGPUDriverVersion, kSmallSize },
    { kGPUPixelShaderVersion, kSmallSize },
    { kGPUVertexShaderVersion, kSmallSize },
    { kGPUVendor, kSmallSize },
    { kGPURenderer, kSmallSize },

    // content/:
    { "bad_message_reason", kSmallSize },
    { "discardable-memory-allocated", kSmallSize },
    { "discardable-memory-free", kSmallSize },
    { "mojo-message-error", kMediumSize },
    { "total-discardable-memory-allocated", kSmallSize },
  };

  std::vector<base::debug::CrashKey> keys(fixed_keys,
                                          fixed_keys + arraysize(fixed_keys));

  return base::debug::InitCrashKeys(&keys.at(0), keys.size(), kChunkMaxLength);
}
}

}  // namespace crash_keys
