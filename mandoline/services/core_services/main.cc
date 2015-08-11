// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/services/core_services/core_services_application_delegate.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"

// TODO(erg): Much of this will be the same between mojo applications. Maybe we
// could centralize this code?
#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "base/rand_util.h"
#include "base/sys_info.h"

// TODO(erg): Much of this was coppied from zygote_main_linux.cc
extern "C" {
void __attribute__((visibility("default"))) MojoSandboxWarm() {
  base::RandUint64();
  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::MaxSharedMemorySize();
  base::SysInfo::NumberOfProcessors();

  // TODO(erg): icu does timezone initialization here.

  // TODO(erg): Perform OpenSSL warmup; it wants access to /dev/urandom.

  // TODO(erg): Initialize SkFontConfigInterface; it has its own odd IPC system
  // which probably must be ported to mojo.
}
}
#endif  // defined(OS_LINUX) && !defined(OS_ANDROID)

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(
      new core_services::CoreServicesApplicationDelegate);
  return runner.Run(shell_handle);
}
