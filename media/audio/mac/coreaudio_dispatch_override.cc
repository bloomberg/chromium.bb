// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mac/coreaudio_dispatch_override.h"

#include <dispatch/dispatch.h>
#include <dlfcn.h>
#include <mach-o/loader.h>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"

namespace {
struct dyld_interpose_tuple {
  template <typename T>
  dyld_interpose_tuple(const T* replacement, const T* replacee)
      : replacement(reinterpret_cast<const void*>(replacement)),
        replacee(reinterpret_cast<const void*>(replacee)) {}
  const void* replacement;
  const void* replacee;
};
}  // namespace

// This method, and the tuple above, is defined in dyld_priv.h; see:
// https://github.com/opensource-apple/dyld/blob/master/include/mach-o/dyld_priv.h
extern "C" void dyld_dynamic_interpose(
    const struct mach_header* mh,
    const struct dyld_interpose_tuple array[],
    size_t count) __attribute__((weak_import));

namespace media {
namespace {
const char kCoreAudioPath[] =
    "/System/Library/Frameworks/CoreAudio.framework/Versions/A/CoreAudio";

dispatch_queue_t g_pause_resume_queue = nullptr;
bool g_dispatch_override_installed = false;
base::subtle::AtomicWord g_resumeio_callsite = 0;
base::subtle::AtomicWord g_pauseio_callsite = 0;

bool AddressIsPauseOrResume(intptr_t address) {
  if (address == 0)
    return false;

  intptr_t resumeio_callsite =
      base::subtle::NoBarrier_Load(&g_resumeio_callsite);

  if (address == resumeio_callsite)
    return true;

  intptr_t pauseio_callsite = base::subtle::NoBarrier_Load(&g_pauseio_callsite);
  if (address == pauseio_callsite)
    return true;

  if (resumeio_callsite && pauseio_callsite)
    return false;

  // We don't know both callsites yet, so try to look up the caller.
  Dl_info info;
  if (!dladdr(reinterpret_cast<const void*>(address), &info))
    return false;

  DCHECK_EQ(strcmp(info.dli_fname, kCoreAudioPath), 0);

  if (!resumeio_callsite && info.dli_sname &&
      strcmp(info.dli_sname, "HALC_IOContext_ResumeIO") == 0) {
    resumeio_callsite = address;
    base::subtle::NoBarrier_CompareAndSwap(&g_resumeio_callsite, 0,
                                           resumeio_callsite);
  }
  if (!pauseio_callsite && info.dli_sname &&
      strcmp(info.dli_sname, "HALC_IOContext_PauseIO") == 0) {
    pauseio_callsite = address;
    base::subtle::NoBarrier_CompareAndSwap(&g_pauseio_callsite, 0,
                                           pauseio_callsite);
  }

  return address == pauseio_callsite || address == resumeio_callsite;
}

dispatch_queue_t GetGlobalQueueOverride(long identifier, unsigned long flags) {
  // Get the return address.
  const intptr_t* rbp = 0;
  asm("movq %%rbp, %0;" : "=r"(rbp));
  const intptr_t caller = rbp[1];

  // Check if it's one we should override.
  if (identifier == DISPATCH_QUEUE_PRIORITY_HIGH &&
      AddressIsPauseOrResume(caller)) {
    return g_pause_resume_queue;
  }

  return dispatch_get_global_queue(identifier, flags);
}

}  // namespace

bool InitializeCoreAudioDispatchOverride() {
  if (g_dispatch_override_installed)
    return true;

  DCHECK_EQ(g_pause_resume_queue, nullptr);

  if (!base::mac::IsAtLeastOS10_10()) {
    return false;
  }

  // This function should be available in macOS > 10.10.
  if (dyld_dynamic_interpose == nullptr) {
    LOG(ERROR) << "Unable to resolve dyld_dynamic_interpose()";
    return false;
  }
  // Get CoreAudio handle
  void* coreaudio = dlopen(kCoreAudioPath, RTLD_LAZY);
  if (!coreaudio) {
    LOG(ERROR) << "Could not load CoreAudio while trying to initialize "
                  "dispatch override";
    return false;
  }
  // Retrieve the base address (also address of Mach header). For this
  // we need any external symbol to look up.
  const void* symbol = dlsym(coreaudio, "AudioObjectGetPropertyData");
  if (!symbol) {
    LOG(ERROR) << "Unable to resolve AudioObjectGetPropertyData in "
                  "CoreAudio library";
    return false;
  }
  // From the address of that symbol, we can get the address of the library's
  // header.
  Dl_info info = {};
  if (!dladdr(symbol, &info)) {
    LOG(ERROR) << "Unable to find Mach header for CoreAudio library.";
    return false;
  }
  const auto* header = reinterpret_cast<const mach_header*>(info.dli_fbase);
  g_pause_resume_queue =
      dispatch_queue_create("org.chromium.CoreAudioPauseResumeQueue", nullptr);
  dyld_interpose_tuple interposition(&GetGlobalQueueOverride,
                                     &dispatch_get_global_queue);
  dyld_dynamic_interpose(header, &interposition, 1);
  g_dispatch_override_installed = true;
  return true;
}

}  // namespace media
