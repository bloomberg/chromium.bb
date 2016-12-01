// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_
#define CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_

#include <memory>
#include <string>

#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"

namespace memory {

// Traces kernel OOM kill events and Low memory kill events (by Chrome
// TabManager).
//
// For OOM kill events, it listens to kernel message (/dev/kmsg) in a blocking
// manner. It runs in a non-joinable thread in order to avoid blocking shutdown.
// There should be only one MemoryKillsMonitor instance globally at any given
// time, otherwise UMA would receive duplicate events.
//
// For Low memory kills events, chrome calls the single global instance of
// MemoryKillsMonitor synchronously. Note that it would be from a browser thread
// other than the listening thread.
//
// For every events, it reports to UMA and optionally a local file specified by
// --memory-kills-log. The log file is useful if we want to analyze low memory
// kills. If the flag is not given, it won't write to any file.
class MemoryKillsMonitor : public base::DelegateSimpleThread::Delegate {
 public:
  // A handle representing the MemoryKillsMonitor's lifetime (the monitor itself
  // can't be destroyed per being a non-joinable Thread).
  class Handle {
   public:
    // Constructs a handle that will flag |outer| as shutting down on
    // destruction.
    explicit Handle(MemoryKillsMonitor* outer);

    Handle(Handle&& handle);

    ~Handle();

   private:
    MemoryKillsMonitor* outer_;
    DISALLOW_COPY_AND_ASSIGN(Handle);
  };

  // Instantiates the MemoryKillsMonitor instance and starts it. This must only
  // be invoked once per process.
  static Handle StartMonitoring();

  // Logs a low memory kill event.
  static void LogLowMemoryKill(const std::string& type, int estimated_freed_kb);

 private:
  FRIEND_TEST_ALL_PREFIXES(MemoryKillsMonitorTest, TryMatchOomKillLine);

  MemoryKillsMonitor();
  ~MemoryKillsMonitor() override;

  // Overridden from base::DelegateSimpleThread::Delegate:
  void Run() override;

  // Try to match a line in kernel message which reports OOM.
  static void TryMatchOomKillLine(const std::string& line);

  // A flag set when MemoryKillsMonitor is shutdown so that its thread can poll
  // it and attempt to wind down from that point (to avoid unnecessary work, not
  // because it blocks shutdown).
  base::AtomicFlag is_shutting_down_;

  // The underlying worker thread which is non-joinable to avoid blocking
  // shutdown.
  std::unique_ptr<base::DelegateSimpleThread> non_joinable_worker_thread_;

  DISALLOW_COPY_AND_ASSIGN(MemoryKillsMonitor);
};

}  // namespace memory

#endif  // CHROME_BROWSER_MEMORY_MEMORY_KILLS_MONITOR_H_
