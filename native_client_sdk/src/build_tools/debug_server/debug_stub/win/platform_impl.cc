/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <exception>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/debug_server/gdb_rsp/abi.h"
#include "native_client/src/debug_server/port/platform.h"

/*
 * Define the OS specific portions of gdb_utils IPlatform interface.
 */
int GetLastErrorString(char* buffer, size_t length) {
  DWORD error = GetLastError();
  return FormatMessageA(
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      error,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      buffer,
      static_cast<DWORD>((64 * 1024 < length) ? 64 * 1024 : length),
      NULL) ? 0 : -1;
}


static DWORD Reprotect(void *ptr, uint32_t len, DWORD newflags) {
  DWORD oldflags;
  if (!VirtualProtect(ptr, len, newflags, &oldflags)) {
    char msg[256] = {0};
    GetLastErrorString(msg, sizeof(msg) - 1);
    msg[sizeof(msg) - 1] = 0;
    printf("Failed with %d [%s]\n", GetLastError(), msg);
    return -1;
  }

  FlushInstructionCache(GetCurrentProcess(), ptr, len);
  return oldflags;
}

namespace port {

// Called to request the platform start/stop the thread
uint32_t IPlatform::GetCurrentThread() {
  return static_cast<uint32_t>(GetCurrentThreadId());
}

/*
 * Since the windows compiler does not use __stdcall by default, we need to
 * modify this function pointer.
 */
typedef unsigned (__stdcall *WinThreadFunc_t)(void *cookie);

uint32_t IPlatform::CreateThread(IPlatform::ThreadFunc_t func, void* cookie) {
  uint32_t id;
  /*
   * We use our own code here instead of NaClThreadCtor because
   * it does not report the thread ID only the handle.
   * TODO(noelallen) - Merge port and platform
   */
  uintptr_t res = _beginthreadex(NULL, 0,
                                 reinterpret_cast<WinThreadFunc_t>(func),
                                 cookie, 0, &id);

  return id;
}

void IPlatform::Relinquish(uint32_t msec) {
  Sleep(msec);
}

bool IPlatform::GetMemory(uint64_t virt, uint32_t len, void *dst) {
  uint32_t oldFlags = Reprotect(reinterpret_cast<void*>(virt),
                                len, PAGE_READONLY);

  if (oldFlags == -1) return false;

  memcpy(dst, reinterpret_cast<void*>(virt), len);
  (void) Reprotect(reinterpret_cast<void*>(virt), len, oldFlags);
  return true;
}

bool IPlatform::SetMemory(uint64_t virt, uint32_t len, void *src) {
  uint32_t oldFlags = Reprotect(reinterpret_cast<void*>(virt),
                                len, PAGE_EXECUTE_READWRITE);

  if (oldFlags == -1) return false;

  memcpy(reinterpret_cast<void*>(virt), src, len);
  FlushInstructionCache(GetCurrentProcess(),
                        reinterpret_cast<void*>(virt), len);
  (void) Reprotect(reinterpret_cast<void*>(virt), len, oldFlags);
  return true;
}

}  // End of port namespace

