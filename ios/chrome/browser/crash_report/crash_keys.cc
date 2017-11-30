// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/crash_keys.h"

#include <stddef.h>

#include "base/debug/crash_logging.h"
#include "base/macros.h"
#include "components/crash/core/common/crash_keys.h"

size_t RegisterChromeIOSCrashKeys() {
  // The following keys may be chunked by the underlying crash logging system,
  // but ultimately constitute a single key-value pair.
  base::debug::CrashKey fixed_keys[] = {
      {crash_keys::kChannel, crash_keys::kSmallSize},
      {crash_keys::kMetricsClientId, crash_keys::kSmallSize},
      {crash_keys::kNumVariations, crash_keys::kSmallSize},
      {crash_keys::kVariations, crash_keys::kHugeSize},
  };

  return base::debug::InitCrashKeys(fixed_keys, arraysize(fixed_keys),
                                    crash_keys::kChunkMaxLength);
}
