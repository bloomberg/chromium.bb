// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#import <Cocoa/Cocoa.h>
#include <crt_externs.h>
#include <errno.h>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#include <mach/task.h>
#include <malloc/malloc.h>
#import <objc/runtime.h>
#include <signal.h>
#include <spawn.h>
#include <sys/event.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <new>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/debug/debugger.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_mach_port.h"
#include "base/posix/eintr_wrapper.h"
#include "base/scoped_clear_errno.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "third_party/apple_apsl/CFBase.h"
#include "third_party/apple_apsl/malloc.h"

namespace base {

void RestoreDefaultExceptionHandler() {
  // This function is tailored to remove the Breakpad exception handler.
  // exception_mask matches s_exception_mask in
  // breakpad/src/client/mac/handler/exception_handler.cc
  const exception_mask_t exception_mask = EXC_MASK_BAD_ACCESS |
                                          EXC_MASK_BAD_INSTRUCTION |
                                          EXC_MASK_ARITHMETIC |
                                          EXC_MASK_BREAKPOINT;

  // Setting the exception port to MACH_PORT_NULL may not be entirely
  // kosher to restore the default exception handler, but in practice,
  // it results in the exception port being set to Apple Crash Reporter,
  // the desired behavior.
  task_set_exception_ports(mach_task_self(), exception_mask, MACH_PORT_NULL,
                           EXCEPTION_DEFAULT, THREAD_STATE_NONE);
}

}  // namespace base
