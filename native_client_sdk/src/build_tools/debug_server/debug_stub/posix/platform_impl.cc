/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <pthread.h>

#include <map>
#include <vector>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/debug_server/gdb_rsp/abi.h"
#include "native_client/src/debug_server/gdb_rsp/util.h"
#include "native_client/src/debug_server/port/event.h"
#include "native_client/src/debug_server/port/platform.h"

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"


/*
 * Define the OS specific portions of gdb_utils IPlatform interface.
 */


// TODO(noelallen) : Add POSIX implementation.  These functions
// represent a minimal implementation to allow the debugging
// code to link and run.
static port::IEvent* GetLaunchEvent() {
  static port::IEvent* event_ = port::IEvent::Allocate();
  return event_;
}

namespace port {

struct StartInfo_t {
  port::IPlatform::ThreadFunc_t func_;
  void *cookie_;
  volatile uint32_t id_;
};

// Get the OS id of this thread
uint32_t IPlatform::GetCurrentThread() {
  return static_cast<uint32_t>(syscall(SYS_gettid));
}

// Use start stub, to record thread id, and signal launcher
static void *StartFunc(void* cookie) {
  StartInfo_t* info = reinterpret_cast<StartInfo_t*>(cookie);
  info->id_ = (uint32_t) syscall(SYS_gettid);

  printf("Started thread...\n");
  GetLaunchEvent()->Signal();
  info->func_(info->cookie_);

  return NULL;
}

uint32_t IPlatform::CreateThread(ThreadFunc_t func, void* cookie) {
  pthread_t thread;
  StartInfo_t info;

  // Setup the thread information
  info.func_ = func;
  info.cookie_ = cookie;

  printf("Creating thread...\n");

  // Redirect to stub and wait for signal before continuing
  if (pthread_create(&thread, NULL, StartFunc, &info) == 0) {
    GetLaunchEvent()->Wait();
    printf("Found thread...\n");
    return info.id_;
  }

  return 0;
}

void IPlatform::Relinquish(uint32_t msec) {
  usleep(msec * 1000);
}

bool IPlatform::GetMemory(uint64_t virt, uint32_t len, void *dst) {
  UNREFERENCED_PARAMETER(virt);
  UNREFERENCED_PARAMETER(len);
  UNREFERENCED_PARAMETER(dst);
  return false;
}

bool IPlatform::SetMemory(uint64_t virt, uint32_t len, void *src) {
  UNREFERENCED_PARAMETER(virt);
  UNREFERENCED_PARAMETER(len);
  UNREFERENCED_PARAMETER(src);
  return false;
}

}  // End of port namespace

