// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/crash_keys.h"

namespace crash_keys {

// TODO(rsesek): This is true on Mac and Linux but not Windows.
static const size_t kSingleChunkLength = 255;

size_t RegisterChromeCrashKeys() {
  base::debug::CrashKey keys[] = {
    // TODO(rsesek): Remove when done testing. Needed so arraysize > 0.
    { "rsesek_key", 1 },
#if defined(OS_MACOSX)
    { mac::kFirstNSException, 1 },
    { mac::kFirstNSExceptionTrace, 1 },
    { mac::kLastNSException, 1 },
    { mac::kLastNSExceptionTrace, 1 },
    { mac::kNSException, 1 },
    { mac::kSendAction, 1 },
    { mac::kZombie, 1 },
    { mac::kZombieTrace, 1 },
    // content/:
    { "channel_error_bt", 1 },
    { "remove_route_bt", 1 },
    { "rwhvm_window", 1 },
    // media/:
    { "VideoCaptureDeviceQTKit", 1 },
#endif
  };

  return base::debug::InitCrashKeys(keys, arraysize(keys), kSingleChunkLength);
}

namespace mac {

const char kFirstNSException[] = "firstexception";
const char kFirstNSExceptionTrace[] = "firstexception_bt";

const char kLastNSException[] = "lastexception";
const char kLastNSExceptionTrace[] = "lastexception_bt";

const char kNSException[] = "nsexception";

const char kSendAction[] = "sendaction";

const char kZombie[] = "zombie";
const char kZombieTrace[] = "zombie_dealloc_bt";

}  // namespace mac

}  // namespace crash_keys
