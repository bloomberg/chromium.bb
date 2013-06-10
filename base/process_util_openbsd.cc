// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/threading/thread_restrictions.h"

namespace base {

ProcessId GetParentProcessId(ProcessHandle process) {
  struct kinfo_proc info;
  size_t length;
  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, process,
                sizeof(struct kinfo_proc), 0 };

  if (sysctl(mib, arraysize(mib), NULL, &length, NULL, 0) < 0)
    return -1;

  mib[5] = (length / sizeof(struct kinfo_proc));

  if (sysctl(mib, arraysize(mib), &info, &length, NULL, 0) < 0)
    return -1;

  return info.p_ppid;
}

FilePath GetProcessExecutablePath(ProcessHandle process) {
  struct kinfo_proc kp;
  size_t len;
  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, process,
                sizeof(struct kinfo_proc), 0 };

  if (sysctl(mib, arraysize(mib), NULL, &len, NULL, 0) == -1)
    return FilePath();
  mib[5] = (len / sizeof(struct kinfo_proc));
  if (sysctl(mib, arraysize(mib), &kp, &len, NULL, 0) < 0)
    return FilePath();
  if ((kp.p_flag & P_SYSTEM) != 0)
    return FilePath();
  if (strcmp(kp.p_comm, "chrome") == 0)
    return FilePath(kp.p_comm);

  return FilePath();
}

void EnableTerminationOnOutOfMemory() {
}

void EnableTerminationOnHeapCorruption() {
}

}  // namespace base
