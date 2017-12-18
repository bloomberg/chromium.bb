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
      {kActiveURL, kLargeSize},
      {kNumVariations, kSmallSize},
      {kVariations, kHugeSize},
      {kShutdownType, kSmallSize},
      {kBrowserUnpinTrace, kMediumSize},

      {kViewCount, kSmallSize},

      // media/:
      {kZeroEncodeDetails, kSmallSize},

      // TODO(sunnyps): Remove after fixing crbug.com/724999.
      {"gl-context-set-current-stack-trace", kMediumSize},
  };

  // This dynamic set of keys is used for sets of key value pairs when gathering
  // a collection of data, like command line switches or extension IDs.
  std::vector<base::debug::CrashKey> keys(fixed_keys,
                                          fixed_keys + arraysize(fixed_keys));

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
    "gpu-gl-vendor__1",
    "gpu-gl-vendor__2",
    "gpu-gl-renderer",

    // content/:
    "bad_message_reason",
    "discardable-memory-allocated",
    "discardable-memory-free",
    "mojo-message-error__1",
    "mojo-message-error__2",
    "mojo-message-error__3",
    "mojo-message-error__4",
    "total-discardable-memory-allocated",
    nullptr};
// clang-format on

void InitCrashKeysForWebViewTesting() {
  breakpad::InitCrashKeysForTesting();
}

}  // namespace crash_keys

}  // namespace android_webview
