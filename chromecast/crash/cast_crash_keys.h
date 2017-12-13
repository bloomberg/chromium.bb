// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CRASH_CAST_CRASH_KEYS_H_
#define CHROMECAST_CRASH_CAST_CRASH_KEYS_H_

#include <stddef.h>

#include "components/crash/core/common/crash_key.h"

namespace chromecast {
namespace crash_keys {

size_t RegisterCastCrashKeys();

extern crash_reporter::CrashKeyString<64> last_app;

extern crash_reporter::CrashKeyString<64> previous_app;

}  // namespace chromecast
}  // namespace crash_keys

#endif  // CHROMECAST_CRASH_CAST_CRASH_KEYS_H_
