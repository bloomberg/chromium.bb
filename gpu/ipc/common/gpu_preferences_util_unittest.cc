// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "build/build_config.h"
#include "gpu/ipc/common/gpu_preferences.mojom.h"
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

// TODO(https://crbug.com/799458): Fix this test.
#if defined(OS_WIN)
#define MAYBE_EncodeDecode DISABLED_EncodeDecode
#else
#define MAYBE_EncodeDecode EncodeDecode
#endif
TEST(GpuPreferencesUtilTest, MAYBE_EncodeDecode) {
  {  // Testing default values.
    ScopedGpuPreferences scoped_input_prefs, scoped_decoded_prefs;
    GpuPreferences& input_prefs = scoped_input_prefs.Ref();
    GpuPreferences& decoded_prefs = scoped_decoded_prefs.Ref();
    std::string encoded = GpuPreferencesToSwitchValue(input_prefs);
    bool flag = SwitchValueToGpuPreferences(encoded, &decoded_prefs);
    EXPECT_TRUE(flag);
    EXPECT_EQ(0, memcmp(&input_prefs, &decoded_prefs, sizeof(input_prefs)));
  }

  {  // Change all fields to non default values.
    ScopedGpuPreferences scoped_input_prefs, scoped_decoded_prefs;
    GpuPreferences& input_prefs = scoped_input_prefs.Ref();
    GpuPreferences& decoded_prefs = scoped_decoded_prefs.Ref();

    GpuPreferences default_prefs;
    mojom::GpuPreferences prefs_mojom;

    // Make sure all feilds are included in mojo struct.
    // TODO(zmo): This test isn't perfect. If a field isn't included in
    // mojom::GpuPreferences, but because of alignment, the two struct sizes
    // could still be the same.
    EXPECT_EQ(sizeof(default_prefs), sizeof(prefs_mojom));

#define GPU_PREFERENCES_FIELD(name, value)         \
  input_prefs.name = value;                        \
  EXPECT_NE(default_prefs.name, input_prefs.name); \
  prefs_mojom.name = value;                        \
  EXPECT_EQ(input_prefs.name, prefs_mojom.name);

    GPU_PREFERENCES_FIELD(single_process, true)
    GPU_PREFERENCES_FIELD(in_process_gpu, true)
    GPU_PREFERENCES_FIELD(disable_accelerated_video_decode, true)
    GPU_PREFERENCES_FIELD(disable_accelerated_video_encode, true)
    GPU_PREFERENCES_FIELD(gpu_startup_dialog, true)
    GPU_PREFERENCES_FIELD(disable_gpu_watchdog, true)
    GPU_PREFERENCES_FIELD(gpu_sandbox_start_early, true)
    GPU_PREFERENCES_FIELD(disable_web_rtc_hw_encoding, true)
    GPU_PREFERENCES_FIELD(enable_accelerated_vpx_decode,
                          GpuPreferences::VPX_VENDOR_AMD)
    GPU_PREFERENCES_FIELD(enable_low_latency_dxva, false)
    GPU_PREFERENCES_FIELD(enable_zero_copy_dxgi_video, true)
    GPU_PREFERENCES_FIELD(enable_nv12_dxgi_video, true)
    GPU_PREFERENCES_FIELD(enable_media_foundation_vea_on_windows7, true)
    GPU_PREFERENCES_FIELD(disable_software_rasterizer, true)
    GPU_PREFERENCES_FIELD(log_gpu_control_list_decisions, true)
    GPU_PREFERENCES_FIELD(compile_shader_always_succeeds, true)
    GPU_PREFERENCES_FIELD(disable_gl_error_limit, true)
    GPU_PREFERENCES_FIELD(disable_glsl_translator, true)
    GPU_PREFERENCES_FIELD(disable_shader_name_hashing, true)
    GPU_PREFERENCES_FIELD(enable_gpu_command_logging, true)
    GPU_PREFERENCES_FIELD(enable_gpu_debugging, true)
    GPU_PREFERENCES_FIELD(enable_gpu_service_logging_gpu, true)
    GPU_PREFERENCES_FIELD(enable_gpu_driver_debug_logging, true)
    GPU_PREFERENCES_FIELD(disable_gpu_program_cache, true)
    GPU_PREFERENCES_FIELD(enforce_gl_minimums, true)
    GPU_PREFERENCES_FIELD(force_gpu_mem_available, 4096)
    GPU_PREFERENCES_FIELD(gpu_program_cache_size,
                          kDefaultMaxProgramCacheMemoryBytes - 1)
    GPU_PREFERENCES_FIELD(disable_gpu_shader_disk_cache, true)
    GPU_PREFERENCES_FIELD(enable_threaded_texture_mailboxes, true)
    GPU_PREFERENCES_FIELD(gl_shader_interm_output, true)
    GPU_PREFERENCES_FIELD(emulate_shader_precision, true)
    GPU_PREFERENCES_FIELD(enable_gpu_service_logging, true)
    GPU_PREFERENCES_FIELD(enable_gpu_service_tracing, true)
    GPU_PREFERENCES_FIELD(use_passthrough_cmd_decoder, true)
    GPU_PREFERENCES_FIELD(disable_biplanar_gpu_memory_buffers_for_video_frames,
                          true)
    GPU_PREFERENCES_FIELD(disable_gpu_driver_bug_workarounds, true)
    GPU_PREFERENCES_FIELD(ignore_gpu_blacklist, true)

    // Make sure every field is encoded/decoded.
    std::string encoded = GpuPreferencesToSwitchValue(input_prefs);
    bool flag = SwitchValueToGpuPreferences(encoded, &decoded_prefs);
    EXPECT_TRUE(flag);
    EXPECT_EQ(0, memcmp(&input_prefs, &decoded_prefs, sizeof(input_prefs)));
  }
}

}  // namespace gpu
