// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/cast_crash_keys.h"

#include "base/debug/crash_logging.h"
#include "components/crash/core/common/crash_keys.h"

namespace chromecast {
namespace crash_keys {

size_t RegisterCastCrashKeys() {
  const base::debug::CrashKey fixed_keys[] = {
      // TODO(sanfin): The following crash keys are copied from
      // chrome/common/crash_keys.cc. When http://crbug.com/598854 is fixed,
      // remove these and refactor as necessary.

      {"url-chunk", ::crash_keys::kLargeSize},
      {::crash_keys::kNumVariations, ::crash_keys::kSmallSize},
      {::crash_keys::kVariations, ::crash_keys::kHugeSize},
      {"num-extensions", ::crash_keys::kSmallSize},
      {"shutdown-type", ::crash_keys::kSmallSize},
      {"browser-unpin-trace", ::crash_keys::kMediumSize},

      {"view-count", ::crash_keys::kSmallSize},

      // media/:
      {"zero-encode-details", ::crash_keys::kSmallSize},

      // TODO(sunnyps): Remove after fixing crbug.com/724999.
      {"gl-context-set-current-stack-trace", ::crash_keys::kMediumSize},
  };

  return base::debug::InitCrashKeys(fixed_keys, arraysize(fixed_keys),
                                    ::crash_keys::kChunkMaxLength);
}

crash_reporter::CrashKeyString<64> last_app("last_app");

crash_reporter::CrashKeyString<64> previous_app("previous_app");

}  // namespace crash_keys
}  // namespace chromecast
