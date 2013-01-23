// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ZYGOTE_HOST_LINUX_H_
#define CONTENT_PUBLIC_BROWSER_ZYGOTE_HOST_LINUX_H_

#include <unistd.h>

#include "base/process.h"
#include "content/common/content_export.h"

namespace content {

// http://code.google.com/p/chromium/wiki/LinuxZygote

// The zygote host is the interface, in the browser process, to the zygote
// process.
class ZygoteHost {
 public:
  // Returns the singleton instance.
  CONTENT_EXPORT static ZygoteHost* GetInstance();

  virtual ~ZygoteHost() {}

  // Returns the pid of the Zygote process.
  virtual pid_t GetPid() const = 0;

  // Returns the pid of the Sandbox Helper process.
  virtual pid_t GetSandboxHelperPid() const = 0;

  // Returns an int which is a bitmask of kSandboxLinux* values. Only valid
  // after the first render has been forked.
  virtual int GetSandboxStatus() const = 0;

  // Adjust the OOM score of the given renderer's PID.  The allowed
  // range for the score is [0, 1000], where higher values are more
  // likely to be killed by the OOM killer.
  virtual void AdjustRendererOOMScore(base::ProcessHandle process_handle,
                                      int score) = 0;

  // Adjust the point at which the low memory notifier in the kernel tells
  // us that we're low on memory.  When there is less than |margin_mb| left,
  // then the notifier will notify us.  Set |margin_mb| to -1 to turn off
  // low memory notification altogether.
  virtual void AdjustLowMemoryMargin(int64 margin_mb) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ZYGOTE_HOST_LINUX_H_
