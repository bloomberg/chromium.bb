// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_viewer.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "third_party/mojo/src/mojo/public/c/system/main.h"

// TODO(erg): Much of this will be the same between mojo applications. Maybe we
// could centralize this code?
#if defined(OS_LINUX) && !defined(OS_ANDROID)
#include "base/rand_util.h"
#include "base/sys_info.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

// TODO(erg): Much of this was coppied from zygote_main_linux.cc
extern "C" {
void __attribute__((visibility("default"))) MojoSandboxWarm() {
  base::RandUint64();
  base::SysInfo::AmountOfPhysicalMemory();
  base::SysInfo::MaxSharedMemorySize();
  base::SysInfo::NumberOfProcessors();

  // ICU DateFormat class (used in base/time_format.cc) needs to get the
  // Olson timezone ID by accessing the zoneinfo files on disk. After
  // TimeZone::createDefault is called once here, the timezone ID is
  // cached and there's no more need to access the file system.
  scoped_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());

  // TODO(erg): Perform OpenSSL warmup; it wants access to /dev/urandom.
}
}
#endif  // defined(OS_LINUX) && !defined(OS_ANDROID)

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new html_viewer::HTMLViewer);
  return runner.Run(shell_handle);
}
