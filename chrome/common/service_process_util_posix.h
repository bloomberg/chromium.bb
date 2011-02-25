// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_

#include "service_process_util.h"

#include <signal.h>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/message_pump_libevent.h"
#include "base/scoped_ptr.h"

#if defined(OS_LINUX)
#include "chrome/common/multi_process_lock.h"
MultiProcessLock* TakeServiceRunningLock(bool waiting);
#endif  // OS_LINUX

#if defined(OS_MACOSX)
#include "base/mac/scoped_cftyperef.h"
class CommandLine;
CFDictionaryRef CreateServiceProcessLaunchdPlist(CommandLine* cmd_line,
                                                 bool for_auto_launch);
#endif  // OS_MACOSX

// Watches for |kShutDownMessage| to be written to the file descriptor it is
// watching. When it reads |kShutDownMessage|, it performs |shutdown_task_|.
// Used here to monitor the socket listening to g_signal_socket.
class ServiceProcessShutdownMonitor
    : public base::MessagePumpLibevent::Watcher {
 public:

  enum {
    kShutDownMessage = 0xdecea5e
  };

  explicit ServiceProcessShutdownMonitor(Task* shutdown_task)
      : shutdown_task_(shutdown_task) {
  }
  virtual ~ServiceProcessShutdownMonitor();

  // base::MessagePumpLibevent::Watcher overrides
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd);

 private:
  scoped_ptr<Task> shutdown_task_;
};

struct ServiceProcessState::StateData
    : public base::RefCountedThreadSafe<ServiceProcessState::StateData> {
#if defined(OS_MACOSX)
  base::mac::ScopedCFTypeRef<CFDictionaryRef> launchd_conf_;
#endif  // OS_MACOSX
#if defined(OS_LINUX)
  scoped_ptr<MultiProcessLock> initializing_lock_;
  scoped_ptr<MultiProcessLock> running_lock_;
#endif  // OS_LINUX
  scoped_ptr<ServiceProcessShutdownMonitor> shut_down_monitor_;
  base::MessagePumpLibevent::FileDescriptorWatcher watcher_;
  int sockets_[2];
  struct sigaction old_action_;
  bool set_action_;

  // WatchFileDescriptor needs to be set up by the thread that is going
  // to be monitoring it.
  void SignalReady();
};

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_
