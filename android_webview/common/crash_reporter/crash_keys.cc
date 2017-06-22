// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/crash_reporter/crash_keys.h"

#include "base/debug/crash_logging.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/core/common/crash_keys.h"
#include "gpu/config/gpu_crash_keys.h"

using namespace crash_keys;

namespace android_webview {
namespace crash_keys {

const char kActiveURL[] = "url-chunk";

const char kFontKeyName[] = "font_key_name";

const char kShutdownType[] = "shutdown-type";
const char kBrowserUnpinTrace[] = "browser-unpin-trace";

const char kAppPackageName[] = "app-package-name";
const char kAppPackageVersionCode[] = "app-package-version-code";

const char kAndroidSdkInt[] = "android-sdk-int";

const char kInputEventFilterSendFailure[] = "input-event-filter-send-failure";

const char kViewCount[] = "view-count";

const char kZeroEncodeDetails[] = "zero-encode-details";

size_t RegisterWebViewCrashKeys() {
  base::debug::CrashKey fixed_keys[] = {
      {"AW_WHITELISTED_DEBUG_KEY", kSmallSize},
      {"AW_NONWHITELISTED_DEBUG_KEY", kSmallSize},
      {kClientId, kSmallSize},
      {kChannel, kSmallSize},
      {kActiveURL, kLargeSize},
      {kNumVariations, kSmallSize},
      {kVariations, kHugeSize},
      {kShutdownType, kSmallSize},
      {kBrowserUnpinTrace, kMediumSize},
      {kAppPackageName, kSmallSize},
      {kAppPackageVersionCode, kSmallSize},
      {kAndroidSdkInt, kSmallSize},

      // gpu
      {gpu::crash_keys::kGPUDriverVersion, kSmallSize},
      {gpu::crash_keys::kGPUPixelShaderVersion, kSmallSize},
      {gpu::crash_keys::kGPUVertexShaderVersion, kSmallSize},
      {gpu::crash_keys::kGPUVendor, kSmallSize},
      {gpu::crash_keys::kGPURenderer, kSmallSize},
      // Temporary for https://crbug.com/729483.
      // TODO(sunnyps): Remove after https://crbug.com/729483 is fixed.
      {gpu::crash_keys::kGpuChannelFilterTrace, kMediumSize},
      {gpu::crash_keys::kMediaGpuChannelFilterTrace, kMediumSize},

      // content/:
      {"bad_message_reason", kSmallSize},
      {"discardable-memory-allocated", kSmallSize},
      {"discardable-memory-free", kSmallSize},
      {kFontKeyName, kSmallSize},
      {"mojo-message-error", kMediumSize},
      {"ppapi_path", kMediumSize},
      {"subresource_url", kLargeSize},
      {"total-discardable-memory-allocated", kSmallSize},
      {kInputEventFilterSendFailure, kSmallSize},
      {kBug464926CrashKey, kSmallSize},
      {kViewCount, kSmallSize},

      // media/:
      {kZeroEncodeDetails, kSmallSize},

      // gin/:
      {"v8-ignition", kSmallSize},

      // sandbox/:
      {"seccomp-sigsys", kMediumSize},

      // Temporary for http://crbug.com/575245.
      {"swapout_frame_id", kSmallSize},
      {"swapout_proxy_id", kSmallSize},
      {"swapout_view_id", kSmallSize},
      {"commit_frame_id", kSmallSize},
      {"commit_proxy_id", kSmallSize},
      {"commit_view_id", kSmallSize},
      {"commit_main_render_frame_id", kSmallSize},
      {"newproxy_proxy_id", kSmallSize},
      {"newproxy_view_id", kSmallSize},
      {"newproxy_opener_id", kSmallSize},
      {"newproxy_parent_id", kSmallSize},
      {"rvinit_view_id", kSmallSize},
      {"rvinit_proxy_id", kSmallSize},
      {"rvinit_main_frame_id", kSmallSize},
      {"initrf_frame_id", kSmallSize},
      {"initrf_proxy_id", kSmallSize},
      {"initrf_view_id", kSmallSize},
      {"initrf_main_frame_id", kSmallSize},
      {"initrf_view_is_live", kSmallSize},

      // Temporary for https://crbug.com/591478.
      {"initrf_parent_proxy_exists", kSmallSize},
      {"initrf_render_view_is_live", kSmallSize},
      {"initrf_parent_is_in_same_site_instance", kSmallSize},
      {"initrf_parent_process_is_live", kSmallSize},
      {"initrf_root_is_in_same_site_instance", kSmallSize},
      {"initrf_root_is_in_same_site_instance_as_parent", kSmallSize},
      {"initrf_root_process_is_live", kSmallSize},
      {"initrf_root_proxy_is_live", kSmallSize},

      // Temporary for https://crbug.com/626802.
      {"newframe_routing_id", kSmallSize},
      {"newframe_proxy_id", kSmallSize},
      {"newframe_opener_id", kSmallSize},
      {"newframe_parent_id", kSmallSize},
      {"newframe_widget_id", kSmallSize},
      {"newframe_widget_hidden", kSmallSize},
      {"newframe_replicated_origin", kSmallSize},
      {"newframe_oopifs_possible", kSmallSize},

      // Temporary for https://crbug.com/612711.
      {"aci_wrong_sp_extension_id", kSmallSize},

      // Temporary for https://crbug.com/668633.
      {"swdh_set_hosted_version_worker_pid", kSmallSize},
      {"swdh_set_hosted_version_host_pid", kSmallSize},
      {"swdh_set_hosted_version_is_new_process", kSmallSize},
      {"swdh_set_hosted_version_restart_count", kSmallSize},
  };

  // This dynamic set of keys is used for sets of key value pairs when gathering
  // a collection of data, like command line switches or extension IDs.
  std::vector<base::debug::CrashKey> keys(fixed_keys,
                                          fixed_keys + arraysize(fixed_keys));

  GetCrashKeysForCommandLineSwitches(&keys);

  return base::debug::InitCrashKeys(&keys.at(0), keys.size(), kChunkMaxLength);
}

// clang-format off
const char* const kWebViewCrashKeyWhiteList[] = {
    "AW_WHITELISTED_DEBUG_KEY",
    kAppPackageName,
    kAppPackageVersionCode,
    kAndroidSdkInt,

    // gpu
    gpu::crash_keys::kGPUDriverVersion,
    gpu::crash_keys::kGPUPixelShaderVersion,
    gpu::crash_keys::kGPUVertexShaderVersion,
    gpu::crash_keys::kGPUVendor,
    gpu::crash_keys::kGPURenderer,

    // content/:
    "bad_message_reason",
    "discardable-memory-allocated",
    "discardable-memory-free",
    "mojo-message-error-1",
    "mojo-message-error-2",
    "mojo-message-error-3",
    "mojo-message-error-4",
    "total-discardable-memory-allocated",
    nullptr};
// clang-format on

void InitCrashKeysForWebViewTesting() {
  breakpad::InitCrashKeysForTesting();
}

}  // namespace crash_keys

}  // namespace android_webview
