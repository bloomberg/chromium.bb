// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_
#define COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_

#include <string>
#include <vector>

#include "build/build_config.h"

namespace crash_keys {

// Sets the ID (which may either be a full GUID or a GUID that was already
// stripped from its dashes -- in either case this method will strip remaining
// dashes before setting the crash key).
void SetMetricsClientIdFromGUID(const std::string& metrics_client_guid);
void ClearMetricsClientId();

// Sets the list of active experiment/variations info.
void SetVariationsList(const std::vector<std::string>& variations);

// Crash Key Constants /////////////////////////////////////////////////////////

// kChunkMaxLength is the platform-specific maximum size that a value in a
// single chunk can be; see base::debug::InitCrashKeys. The maximum lengths
// specified by breakpad include the trailing NULL, so the actual length of the
// chunk is one less.
#if defined(OS_MACOSX)
const size_t kChunkMaxLength = 255;
#else  // OS_MACOSX
const size_t kChunkMaxLength = 63;
#endif  // !OS_MACOSX

// A small crash key, guaranteed to never be split into multiple pieces.
const size_t kSmallSize = 63;

// A medium crash key, which will be chunked on certain platforms but not
// others. Guaranteed to never be more than four chunks.
const size_t kMediumSize = kSmallSize * 4;

// A large crash key, which will be chunked on all platforms. This should be
// used sparingly.
const size_t kLargeSize = kSmallSize * 16;

// Crash Key Name Constants ////////////////////////////////////////////////////

// The GUID used to identify this client to the crash system.
#if defined(OS_MACOSX)
// On Mac OS X, the crash reporting client ID is the responsibility of Crashpad.
// It is not set directly by Chrome. To make the metrics client ID available on
// the server, it's stored in a distinct key.
extern const char kMetricsClientId[];
#else
// When using Breakpad instead of Crashpad, the crash reporting client ID is the
// same as the metrics client ID.
extern const char kClientId[];
#endif

// The product release/distribution channel.
extern const char kChannel[];

// The total number of experiments the instance has.
extern const char kNumVariations[];

// The experiments chunk. Hashed experiment names separated by |,|. This is
// typically set by SetExperimentList.
extern const char kVariations[];

// Used to help investigate bug 464926.
extern const char kBug464926CrashKey[];

#if defined(OS_MACOSX)
namespace mac {

// Records Cocoa zombie/used-after-freed objects that resulted in a
// deliberate crash.
extern const char kZombie[];
extern const char kZombieTrace[];

}  // namespace mac
#endif

}  // namespace crash_keys

#endif  // COMPONENTS_CRASH_CORE_COMMON_CRASH_KEYS_H_
