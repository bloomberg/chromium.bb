// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/simple_broker.h"

#include "base/process/process.h"
#include "mojo/edk/embedder/platform_channel_pair.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace mojo {
namespace edk {

SimpleBroker::SimpleBroker() {
}

SimpleBroker::~SimpleBroker() {
}

#if defined(OS_WIN)
void SimpleBroker::CreatePlatformChannelPair(
    ScopedPlatformHandle* server, ScopedPlatformHandle* client) {
  PlatformChannelPair channel_pair;
  *server = channel_pair.PassServerHandle();
  *client = channel_pair.PassClientHandle();
}

void SimpleBroker::HandleToToken(const PlatformHandle* platform_handles,
                                 size_t count,
                                 uint64_t* tokens) {
  // Since we're not sure which process might ultimately deserialize the message
  // we can't duplicate the handle now. Instead, write the process ID and handle
  // now and let the receiver duplicate it.
  uint32_t current_process_id = base::GetCurrentProcId();
  for (size_t i = 0; i < count; ++i) {
    tokens[i] = current_process_id;
    tokens[i] = tokens[i] << 32;
    // Windows HANDLES are always 32 bit per
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa384203(v=vs.85).aspx
#if defined(_WIN64)
    tokens[i] |= static_cast<int32_t>(
        reinterpret_cast<int64_t>(platform_handles[i].handle));
#else
    tokens[i] |= reinterpret_cast<int32_t>(platform_handles[i].handle);
#endif
  }
}

void SimpleBroker::TokenToHandle(const uint64_t* tokens,
                                 size_t count,
                                 PlatformHandle* handles) {
  for (size_t i = 0; i < count; ++i) {
    DWORD pid = tokens[i] >> 32;
#if defined(_WIN64)
    // Sign-extend to preserve INVALID_HANDLE_VALUE.
    HANDLE source_handle = reinterpret_cast<HANDLE>(
        static_cast<int64_t>(static_cast<int32_t>(tokens[i] & 0xFFFFFFFF)));
#else
    HANDLE source_handle = reinterpret_cast<HANDLE>(tokens[i] & 0xFFFFFFFF);
#endif
    base::Process sender =
        base::Process::OpenWithAccess(pid, PROCESS_DUP_HANDLE);
    handles[i] = PlatformHandle();
    if (!sender.IsValid()) {
      // Sender died.
    } else {
      BOOL dup_result = DuplicateHandle(
          sender.Handle(), source_handle,
          base::GetCurrentProcessHandle(), &handles[i].handle, 0,
          FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
      DCHECK(dup_result);
    }
  }
}
#endif

}  // namespace edk
}  // namespace mojo
