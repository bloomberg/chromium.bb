// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_kills_monitor.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/safe_strerror.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/synchronization/atomic_flag.h"
#include "base/time/time.h"
#include "chrome/browser/memory/memory_kills_histogram.h"
#include "third_party/re2/src/re2/re2.h"

namespace memory {

using base::SequencedWorkerPool;
using base::TimeDelta;

namespace {

int64_t GetTimestamp(const std::string& line) {
  std::vector<std::string> fields = base::SplitString(
      line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  int64_t timestamp = -1;
  // Timestamp is the third field in a line of /dev/kmsg.
  if (fields.size() < 3 || !base::StringToInt64(fields[2], &timestamp))
    return -1;
  return timestamp;
}

void LogEvent(const base::Time& time_stamp, const std::string& event) {
  VLOG(1) << time_stamp.ToJavaTime() << ", " << event;
}

void LogOOMKill(int64_t time_stamp, int oom_badness) {
  static int64_t last_kill_time = -1;
  static int oom_kills = 0;

  // Ideally the timestamp should be parsed from /dev/kmsg, but the timestamp
  // there is the elapsed time since system boot. So the timestamp |now| used
  // here is a bit delayed.
  base::Time now = base::Time::Now();
  LogEvent(now, "OOM_KILL");

  ++oom_kills;
  // Report the cumulative count of killed process in one login session.
  // For example if there are 3 processes killed, it would report 1 for the
  // first kill, 2 for the second kill, then 3 for the final kill.
  // It doesn't report a final count at the end of a user session because
  // the code runs in a dedicated thread and never ends until browser shutdown
  // (or logout on Chrome OS). And on browser shutdown the thread may be
  // terminated brutally so there's no chance to execute a "final" block.
  // More specifically, code outside the main loop of MemoryKillsMonitor::Run()
  // are not guaranteed to be executed.
  UMA_HISTOGRAM_CUSTOM_COUNTS("Arc.OOMKills.Count", oom_kills, 1, 1000, 1001);

  // In practice most process has oom_badness < 1000, but
  // strictly speaking the number could be [1, 2000]. What it really
  // means is the baseline, proportion of memory used (normalized to
  // [0, 1000]), plus an adjustment score oom_score_adj [-1000, 1000],
  // truncated to 1 if negative (0 means never kill).
  // Ref: https://lwn.net/Articles/396552/
  UMA_HISTOGRAM_CUSTOM_COUNTS("Arc.OOMKills.Score", oom_badness, 1, 2000, 2001);

  if (time_stamp > 0) {
    // Sets to |kMaxMemoryKillTimeDelta| for the first kill event.
    const TimeDelta time_delta =
        last_kill_time < 0 ? kMaxMemoryKillTimeDelta:
        TimeDelta::FromMicroseconds(time_stamp - last_kill_time);

    last_kill_time = time_stamp;

    UMA_HISTOGRAM_MEMORY_KILL_TIME_INTERVAL(
        "Arc.OOMKills.TimeDelta", time_delta);
  }
}

}  // namespace

MemoryKillsMonitor::Handle::Handle(MemoryKillsMonitor* outer) : outer_(outer) {
  DCHECK(outer_);
}

MemoryKillsMonitor::Handle::Handle(MemoryKillsMonitor::Handle&& other)
    : outer_(nullptr) {
  outer_ = other.outer_;
  other.outer_ = nullptr;
}

MemoryKillsMonitor::Handle::~Handle() {
  if (outer_) {
    VLOG(2) << "Chrome is shutting down" << outer_;
    outer_->is_shutting_down_.Set();
  }
}

MemoryKillsMonitor::MemoryKillsMonitor() {
  base::SimpleThread::Options non_joinable_options;
  non_joinable_options.joinable = false;
  non_joinable_worker_thread_ = base::MakeUnique<base::DelegateSimpleThread>(
      this, "memory_kills_monitor", non_joinable_options);
  non_joinable_worker_thread_->Start();
}

MemoryKillsMonitor::~MemoryKillsMonitor() {
  // The instance has to be leaked on shutdown as it is referred to by a
  // non-joinable thread but ~MemoryKillsMonitor() can't be explicitly deleted
  // as it overrides ~SimpleThread(), it should nevertheless never be invoked.
  NOTREACHED();
}

// static
MemoryKillsMonitor::Handle MemoryKillsMonitor::StartMonitoring() {
#if DCHECK_IS_ON()
  static volatile bool monitoring_active = false;
  DCHECK(!monitoring_active);
  monitoring_active = true;
#endif

  // Instantiate the MemoryKillsMonitor and its underlying thread. The
  // MemoryKillsMonitor itself has to be leaked on shutdown per having a
  // non-joinable thread associated to its state. The MemoryKillsMonitor::Handle
  // will notify the MemoryKillsMonitor when it is destroyed so that the
  // underlying thread can at a minimum not do extra work during shutdown.
  MemoryKillsMonitor* instance = new MemoryKillsMonitor();
  ANNOTATE_LEAKING_OBJECT_PTR(instance);
  return Handle(instance);
}

// static
void MemoryKillsMonitor::LogLowMemoryKill(
    const std::string& type, int estimated_freed_kb) {
  static base::Time last_kill_time;
  static int low_memory_kills = 0;

  base::Time now = base::Time::Now();
  LogEvent(now, "LOW_MEMORY_KILL_" + type);

  const TimeDelta time_delta =
      last_kill_time.is_null() ?
      kMaxMemoryKillTimeDelta :
      (now - last_kill_time);
  UMA_HISTOGRAM_MEMORY_KILL_TIME_INTERVAL(
            "Arc.LowMemoryKiller.TimeDelta", time_delta);
  last_kill_time = now;

  ++low_memory_kills;
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Arc.LowMemoryKiller.Count", low_memory_kills, 1, 1000, 1001);

  UMA_HISTOGRAM_MEMORY_KB("Arc.LowMemoryKiller.FreedSize",
                          estimated_freed_kb);
}

// static
void MemoryKillsMonitor::TryMatchOomKillLine(const std::string& line) {
  // Sample OOM log line:
  // 3,1362,97646497541,-;Out of memory: Kill process 29582 (android.vending)
  // score 961 or sacrifice child.
  int oom_badness;
  TimeDelta time_delta;
  if (RE2::PartialMatch(line,
                        "Out of memory: Kill process .* score (\\d+)",
                        &oom_badness)) {
    int64_t time_stamp = GetTimestamp(line);
    LogOOMKill(time_stamp, oom_badness);
  }
}

void MemoryKillsMonitor::Run() {
  VLOG(1) << "MemoryKillsMonitor started";
  base::ScopedFILE kmsg_handle(
      base::OpenFile(base::FilePath("/dev/kmsg"), "r"));
  if (!kmsg_handle) {
    LOG(WARNING) << "Open /dev/kmsg failed: " << base::safe_strerror(errno);
    return;
  }
  // Skip kernel messages prior to the instantiation of this object to avoid
  // double reporting.
  fseek(kmsg_handle.get(), 0, SEEK_END);

  static constexpr int kMaxBufSize = 512;
  char buf[kMaxBufSize];

  while (fgets(buf, kMaxBufSize, kmsg_handle.get())) {
    if (is_shutting_down_.IsSet()) {
      // Not guaranteed to execute when the process is shutting down,
      // because the thread might be blocked in fgets().
      VLOG(1) << "Chrome is shutting down, MemoryKillsMonitor exits.";
      break;
    }
    TryMatchOomKillLine(buf);
  }
}


}  // namespace memory
