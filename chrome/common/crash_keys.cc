// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

#if defined(OS_MACOSX)
#include "breakpad/src/common/mac/SimpleStringDictionary.h"
#elif defined(OS_WIN)
#include "breakpad/src/client/windows/common/ipc_protocol.h"
#endif

namespace crash_keys {

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
#if defined(OS_MACOSX)
static const size_t kSingleChunkLength =
    google_breakpad::KeyValueEntry::MAX_STRING_STORAGE_SIZE - 1;
#elif defined(OS_WIN)
static const size_t kSingleChunkLength =
    google_breakpad::CustomInfoEntry::kValueMaxLength - 1;
#else
static const size_t kSingleChunkLength = 63;
#endif

// Guarantees for crash key sizes.
COMPILE_ASSERT(kSmallSize <= kSingleChunkLength,
               crash_key_chunk_size_too_small);
#if defined(OS_MACOSX)
COMPILE_ASSERT(kMediumSize <= kSingleChunkLength,
               mac_has_medium_size_crash_key_chunks);
#endif

size_t RegisterChromeCrashKeys() {
  base::debug::CrashKey keys[] = {
    // TODO(rsesek): Remove when done testing. Needed so arraysize > 0.
    { "rsesek_key", kSmallSize },
    // content/:
    { "ppapi_path", kMediumSize },
#if defined(OS_MACOSX)
    { mac::kFirstNSException, kMediumSize },
    { mac::kFirstNSExceptionTrace, kMediumSize },
    { mac::kLastNSException, kMediumSize },
    { mac::kLastNSExceptionTrace, kMediumSize },
    { mac::kNSException, kMediumSize },
    { mac::kNSExceptionTrace, kMediumSize },
    { mac::kSendAction, kMediumSize },
    { mac::kZombie, kMediumSize },
    { mac::kZombieTrace, kMediumSize },
    // content/:
    { "channel_error_bt", kMediumSize },
    { "remove_route_bt", kMediumSize },
    { "rwhvm_window", kMediumSize },
    // media/:
    { "VideoCaptureDeviceQTKit", kSmallSize },
#endif
  };

  return base::debug::InitCrashKeys(keys, arraysize(keys), kSingleChunkLength);
}

#if defined(OS_MACOSX)
namespace mac {

const char kFirstNSException[] = "firstexception";
const char kFirstNSExceptionTrace[] = "firstexception_bt";

const char kLastNSException[] = "lastexception";
const char kLastNSExceptionTrace[] = "lastexception_bt";

const char kNSException[] = "nsexception";
const char kNSExceptionTrace[] = "nsexception_bt";

const char kSendAction[] = "sendaction";

const char kZombie[] = "zombie";
const char kZombieTrace[] = "zombie_dealloc_bt";

}  // namespace mac
#endif

}  // namespace crash_keys
