// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/crash_reporter/crash_keys.h"

#include "base/debug/crash_logging.h"
#include "components/crash/content/app/breakpad_linux.h"
#include "components/crash/core/common/crash_keys.h"

using namespace crash_keys;

namespace android_webview {
namespace crash_keys {

const char kActiveURL[] = "url-chunk";

const char kShutdownType[] = "shutdown-type";
const char kBrowserUnpinTrace[] = "browser-unpin-trace";

const char kAppPackageName[] = "app-package-name";
const char kAppPackageVersionCode[] = "app-package-version-code";

const char kAndroidSdkInt[] = "android-sdk-int";

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

      // content/:
      {"discardable-memory-allocated", kSmallSize},
      {"discardable-memory-free", kSmallSize},
      {"subresource_url", kLargeSize},
      {"total-discardable-memory-allocated", kSmallSize},
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

      // Temporary for https://crbug.com/685996.
      {"user-cloud-policy-manager-connect-trace", kMediumSize},

      // TODO(sunnyps): Remove after fixing crbug.com/724999.
      {"gl-context-set-current-stack-trace", kMediumSize},
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
    "gpu-driver",
    "gpu-psver",
    "gpu-vsver",
    "gpu-gl-vendor",
    "gpu-gl-renderer",

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
