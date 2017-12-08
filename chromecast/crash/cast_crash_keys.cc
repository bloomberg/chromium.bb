// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/cast_crash_keys.h"

#include "components/crash/core/common/crash_keys.h"

namespace chromecast {
namespace crash_keys {

const char kLastApp[] = "last_app";
const char kCurrentApp[] = "current_app";
const char kPreviousApp[] = "previous_app";

size_t RegisterCastCrashKeys() {
  const base::debug::CrashKey fixed_keys[] = {
      {kLastApp, ::crash_keys::kSmallSize},
      {kCurrentApp, ::crash_keys::kSmallSize},
      {kPreviousApp, ::crash_keys::kSmallSize},

      // TODO(sanfin): The following crash keys are copied from
      // chrome/common/crash_keys.cc. When http://crbug.com/598854 is fixed,
      // remove these and refactor as necessary.

      {::crash_keys::kClientId, ::crash_keys::kSmallSize},
      {::crash_keys::kChannel, ::crash_keys::kSmallSize},
      {"url-chunk", ::crash_keys::kLargeSize},
      {::crash_keys::kNumVariations, ::crash_keys::kSmallSize},
      {::crash_keys::kVariations, ::crash_keys::kHugeSize},
      {"num-extensions", ::crash_keys::kSmallSize},
      {"shutdown-type", ::crash_keys::kSmallSize},
      {"browser-unpin-trace", ::crash_keys::kMediumSize},

      // content/:
      {"discardable-memory-allocated", ::crash_keys::kSmallSize},
      {"discardable-memory-free", ::crash_keys::kSmallSize},
      {"subresource_url", ::crash_keys::kLargeSize},
      {"total-discardable-memory-allocated", ::crash_keys::kSmallSize},
      {"view-count", ::crash_keys::kSmallSize},

      // media/:
      {"zero-encode-details", ::crash_keys::kSmallSize},

      // Site isolation.  These keys help debug renderer kills such as
      // https://crbug.com/773140.
      {"requested_site_url", ::crash_keys::kSmallSize},
      {"requested_origin", ::crash_keys::kSmallSize},
      {"killed_process_origin_lock", ::crash_keys::kSmallSize},
      {"site_isolation_mode", ::crash_keys::kSmallSize},

      // Temporary for https://crbug.com/685996.
      {"user-cloud-policy-manager-connect-trace", ::crash_keys::kMediumSize},

      // TODO(sunnyps): Remove after fixing crbug.com/724999.
      {"gl-context-set-current-stack-trace", ::crash_keys::kMediumSize},
  };

  return base::debug::InitCrashKeys(fixed_keys, arraysize(fixed_keys),
                                    ::crash_keys::kChunkMaxLength);
}

}  // namespace crash_keys
}  // namespace chromecast
