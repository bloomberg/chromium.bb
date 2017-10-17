// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "build/build_config.h"
#include "gpu/ipc/common/gpu_preferences_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

class ScopedGpuPreferences {
 public:
  ScopedGpuPreferences() {
    // To make sure paddings are zeroed so we can use memcmp() in the tests.
    memset(buffer_, 0, sizeof(buffer_));
    prefs_ = new (buffer_) GpuPreferences();
  }

  ~ScopedGpuPreferences() { prefs_->~GpuPreferences(); }

  GpuPreferences& Ref() { return *prefs_; }

 private:
  GpuPreferences* prefs_;
  alignas(GpuPreferences) char buffer_[sizeof(GpuPreferences)];
};

}  // namespace

TEST(GpuPreferencesUtilTest, EncodeDecode) {
  {  // Testing default values.
    ScopedGpuPreferences scoped_input_prefs, scoped_decoded_prefs;
    GpuPreferences& input_prefs = scoped_input_prefs.Ref();
    GpuPreferences& decoded_prefs = scoped_decoded_prefs.Ref();
    std::string encoded = GpuPreferencesToSwitchValue(input_prefs);
    bool flag = SwitchValueToGpuPreferences(encoded, &decoded_prefs);
    EXPECT_TRUE(flag);
    EXPECT_EQ(0, memcmp(&input_prefs, &decoded_prefs, sizeof(input_prefs)));
  }

  {  // Randomly change some fields.
    ScopedGpuPreferences scoped_input_prefs, scoped_decoded_prefs;
    GpuPreferences& input_prefs = scoped_input_prefs.Ref();
    GpuPreferences& decoded_prefs = scoped_decoded_prefs.Ref();
    input_prefs.enable_gpu_scheduler = true;
    input_prefs.disable_gpu_program_cache = true;
    input_prefs.force_gpu_mem_available = 4096;
#if defined(OS_WIN)
    input_prefs.enable_accelerated_vpx_decode = GpuPreferences::VPX_VENDOR_AMD;
#endif
    std::string encoded = GpuPreferencesToSwitchValue(input_prefs);
    bool flag = SwitchValueToGpuPreferences(encoded, &decoded_prefs);
    EXPECT_TRUE(flag);
    EXPECT_EQ(0, memcmp(&input_prefs, &decoded_prefs, sizeof(input_prefs)));
  }
}

}  // namespace gpu
