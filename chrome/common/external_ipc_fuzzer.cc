// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/external_ipc_fuzzer.h"

#if defined(OS_LINUX)
#include <dlfcn.h>
#endif

typedef IPC::ChannelProxy::OutgoingMessageFilter *(*GetFuzzerFunction)();
const char kFuzzLibraryName[] = "libipcfuzz.so";
const char kFuzzEntryName[] = "GetFilter";

IPC::ChannelProxy::OutgoingMessageFilter* LoadExternalIPCFuzzer() {
  IPC::ChannelProxy::OutgoingMessageFilter* result = NULL;

#if defined(OS_LINUX)

  // Fuzz is currently linux-only feature
  void *fuzz_library =  dlopen(kFuzzLibraryName, RTLD_NOW);
  if (fuzz_library) {
    GetFuzzerFunction fuzz_entry_point =
        reinterpret_cast<GetFuzzerFunction>(
            dlsym(fuzz_library, kFuzzEntryName));

    if (fuzz_entry_point)
      result = fuzz_entry_point();
  }

  if (!result)
    LOG(WARNING) << dlerror() << "\n";

#endif // OS_LINUX

  return result;
}



