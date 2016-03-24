// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_engine_crash_keys.h"

#include <vector>

#include "base/debug/crash_logging.h"
#include "components/crash/core/common/crash_keys.h"

namespace blimp {
namespace engine {

size_t RegisterEngineCrashKeys() {
  const std::vector<base::debug::CrashKey> engine_keys = {
      {::crash_keys::kClientId, ::crash_keys::kSmallSize},
      {::crash_keys::kChannel, ::crash_keys::kSmallSize},
      {::crash_keys::kNumVariations, ::crash_keys::kSmallSize},
      {::crash_keys::kVariations, ::crash_keys::kLargeSize},
      {"discardable-memory-allocated", ::crash_keys::kSmallSize},
      {"discardable-memory-free", ::crash_keys::kSmallSize},
      {"ppapi_path", ::crash_keys::kMediumSize},
      {"subresource_url", ::crash_keys::kLargeSize},
      {"total-discardable-memory-allocated", ::crash_keys::kSmallSize}};

  return base::debug::InitCrashKeys(&engine_keys.at(0), engine_keys.size(),
                                    ::crash_keys::kChunkMaxLength);
}

}  // namespace engine
}  // namespace blimp
