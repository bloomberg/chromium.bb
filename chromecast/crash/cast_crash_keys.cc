// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chromecast/crash/cast_crash_keys.h"

// TODO(kjoswiak): Potentially refactor chunk size info as well as non-cast
// specific keys out and make shared with chrome/common/crash_keys.cc.
namespace {

// A small crash key, guaranteed to never be split into multiple pieces.
const size_t kSmallSize = 63;

// A medium crash key, which will be chunked on certain platforms but not
// others. Guaranteed to never be more than four chunks.
const size_t kMediumSize = kSmallSize * 4;

// A large crash key, which will be chunked on all platforms. This should be
// used sparingly.
const size_t kLargeSize = kSmallSize * 16;

// The maximum lengths specified by breakpad include the trailing NULL, so
// the actual length of the string is one less.
static const size_t kChunkMaxLength = 63;

}  // namespace

namespace chromecast {
namespace crash_keys {

const char kLastApp[] = "last_app";
const char kCurrentApp[] = "current_app";
const char kPreviousApp[] = "previous_app";

size_t RegisterCastCrashKeys() {
  const base::debug::CrashKey fixed_keys[] = {
    { kLastApp, kSmallSize },
    { kCurrentApp, kSmallSize },
    { kPreviousApp, kSmallSize },
    // content/:
    { "discardable-memory-allocated", kSmallSize },
    { "discardable-memory-free", kSmallSize },
    { "ppapi_path", kMediumSize },
    { "subresource_url", kLargeSize },
    { "total-discardable-memory-allocated", kSmallSize },
  };

  return base::debug::InitCrashKeys(fixed_keys, arraysize(fixed_keys),
                                    kChunkMaxLength);
}

}  // namespace crash_keys
}  // namespace chromecast
