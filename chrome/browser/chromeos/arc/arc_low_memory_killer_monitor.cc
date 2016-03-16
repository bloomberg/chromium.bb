// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_low_memory_killer_monitor.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/safe_strerror.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "third_party/re2/src/re2/re2.h"

namespace arc {

using base::SequencedWorkerPool;
using base::StringPiece;
using base::TimeDelta;

#define MAX_LOWMEMORYKILL_TIME_SECS 30
#define UMA_HISTOGRAM_LOWMEMORYKILL_TIMES(name, sample)   \
  UMA_HISTOGRAM_CUSTOM_TIMES(                             \
      name, sample, base::TimeDelta::FromMilliseconds(1), \
      base::TimeDelta::FromSeconds(MAX_LOWMEMORYKILL_TIME_SECS), 50)

ArcLowMemoryKillerMonitor::ArcLowMemoryKillerMonitor()
    : worker_pool_(
          new SequencedWorkerPool(1, "arc_low_memory_killer_monitor")) {}

ArcLowMemoryKillerMonitor::~ArcLowMemoryKillerMonitor() {
  Stop();
}

void ArcLowMemoryKillerMonitor::Start() {
  auto task_runner = worker_pool_->GetTaskRunnerWithShutdownBehavior(
      SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);
  task_runner->PostTask(
      FROM_HERE, base::Bind(&ArcLowMemoryKillerMonitor::Run, worker_pool_));
}

void ArcLowMemoryKillerMonitor::Stop() {
  worker_pool_->Shutdown();
}

// static
void ArcLowMemoryKillerMonitor::Run(
    scoped_refptr<base::SequencedWorkerPool> worker_pool) {
  static const int kMaxBufSize = 512;

  FILE* kmsg_handle = base::OpenFile(base::FilePath("/dev/kmsg"), "r");
  if (!kmsg_handle) {
    LOG(WARNING) << "Open /dev/kmsg failed: " << base::safe_strerror(errno);
    return;
  }
  // Skip kernel messages prior to the instantiation of this object to avoid
  // double reporting.
  fseek(kmsg_handle, 0, SEEK_END);

  char buf[kMaxBufSize];
  int freed_size;
  int64_t timestamp, last_timestamp = -1;
  const TimeDelta kMaxTimeDelta =
      TimeDelta::FromSeconds(MAX_LOWMEMORYKILL_TIME_SECS);

  while (fgets(buf, kMaxBufSize, kmsg_handle)) {
    if (worker_pool->IsShutdownInProgress()) {
      DVLOG(1) << "Chrome is shutting down, exit now.";
      break;
    }
    if (RE2::PartialMatch(buf, "lowmemorykiller: .* to free (\\d+)kB",
                          &freed_size)) {
      std::vector<StringPiece> fields = SplitStringPiece(
          buf, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

      // The time to last kill event. Could be |kMaxTimeDelta| in case of the
      // first kill event.
      TimeDelta time_delta(kMaxTimeDelta);
      // Timestamp is the third field in a line of /dev/kmsg.
      //
      // Sample log line:
      // 6,2302,533604004,-;lowmemorykiller: Killing 'externalstorage' (21742),
      // adj 1000,\x0a   to free 27320kB on behalf of 'kswapd0' (47) because\x0a
      // cache 181920kB is below limit 184320kB for oom_score_adj 1000\x0a
      // Free memory is 1228kB above reserved
      if (fields.size() >= 3) {
        base::StringToInt64(fields[2], &timestamp);
        if (last_timestamp > 0) {
          time_delta = TimeDelta::FromMicroseconds(timestamp - last_timestamp);
        }
        last_timestamp = timestamp;

        UMA_HISTOGRAM_MEMORY_KB("ArcRuntime.LowMemoryKiller.FreedSize",
                                freed_size);
        UMA_HISTOGRAM_LOWMEMORYKILL_TIMES(
            "ArcRuntime.LowMemoryKiller.TimeDelta", time_delta);
      }
    }
  }
  base::CloseFile(kmsg_handle);
}

}  // namespace arc
