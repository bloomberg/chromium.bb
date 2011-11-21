// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_

#include "service_process_util.h"

#include <signal.h>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "chrome/common/multi_process_lock.h"
MultiProcessLock* TakeServiceRunningLock(bool waiting);
#endif

#if defined(OS_MACOSX)
#include "base/files/file_path_watcher.h"
#include "base/mac/scoped_cftyperef.h"

class CommandLine;
CFDictionaryRef CreateServiceProcessLaunchdPlist(CommandLine* cmd_line,
                                                 bool for_auto_launch);
#endif  // OS_MACOSX

namespace base {
class WaitableEvent;
}

// Watches for |kTerminateMessage| to be written to the file descriptor it is
// watching. When it reads |kTerminateMessage|, it performs |terminate_task_|.
// Used here to monitor the socket listening to g_signal_socket.
class ServiceProcessTerminateMonitor
    : public MessageLoopForIO::Watcher {
 public:

  enum {
    kTerminateMessage = 0xdecea5e
  };

  explicit ServiceProcessTerminateMonitor(const base::Closure& terminate_task);
  virtual ~ServiceProcessTerminateMonitor();

  // MessageLoopForIO::Watcher overrides
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE;

 private:
  base::Closure terminate_task_;
};

struct ServiceProcessState::StateData
    : public base::RefCountedThreadSafe<ServiceProcessState::StateData> {
  StateData();

  // WatchFileDescriptor needs to be set up by the thread that is going
  // to be monitoring it.
  void SignalReady(base::WaitableEvent* signal, bool* success);


  // TODO(jhawkins): Either make this a class or rename these public member
  // variables to remove the trailing underscore.

#if defined(OS_MACOSX)
  bool WatchExecutable();

  base::mac::ScopedCFTypeRef<CFDictionaryRef> launchd_conf_;
  base::files::FilePathWatcher executable_watcher_;
#endif  // OS_MACOSX
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  scoped_ptr<MultiProcessLock> initializing_lock_;
  scoped_ptr<MultiProcessLock> running_lock_;
#endif
  scoped_ptr<ServiceProcessTerminateMonitor> terminate_monitor_;
  MessageLoopForIO::FileDescriptorWatcher watcher_;
  int sockets_[2];
  struct sigaction old_action_;
  bool set_action_;

 protected:
  friend class base::RefCountedThreadSafe<ServiceProcessState::StateData>;
  virtual ~StateData();
};

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_POSIX_H_
