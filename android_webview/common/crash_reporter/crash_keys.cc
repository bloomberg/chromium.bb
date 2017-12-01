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
      {gpu::crash_keys::kGPUGLContextIsVirtual, kSmallSize},

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
      {kViewCount, kSmallSize},

      // media/:
      {kZeroEncodeDetails, kSmallSize},

      // sandbox/:
      {"seccomp-sigsys", kMediumSize},

      // Site isolation.  These keys help debug renderer kills such as
      // https://crbug.com/773140.
      {"requested_site_url", kSmallSize},
      {"requested_origin", kSmallSize},
      {"killed_process_origin_lock", kSmallSize},
      {"site_isolation_mode", kSmallSize},

      // Temporary for https://crbug.com/626802.
      {"newframe_routing_id", kSmallSize},
      {"newframe_proxy_id", kSmallSize},
      {"newframe_opener_id", kSmallSize},
      {"newframe_parent_id", kSmallSize},
      {"newframe_widget_id", kSmallSize},
      {"newframe_widget_hidden", kSmallSize},
      {"newframe_replicated_origin", kSmallSize},

      // Temporary for https://crbug.com/685996.
      {"user-cloud-policy-manager-connect-trace", kMediumSize},

      // TODO(sunnyps): Remove after fixing crbug.com/724999.
      {"gl-context-set-current-stack-trace", kMediumSize},

      // Accessibility keys. Temporary for http://crbug.com/765490.
      {"ax_tree_error", kSmallSize},
      {"ax_tree_update", kMediumSize},
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
