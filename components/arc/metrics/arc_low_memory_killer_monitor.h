// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_METRICS_ARC_LOW_MEMORY_KILLER_MONITOR_H_
#define COMPONENTS_ARC_METRICS_ARC_LOW_MEMORY_KILLER_MONITOR_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/sequenced_worker_pool.h"

namespace arc {

// Trace lowmemorykiller events and report to UMA.
//
// If kernel lowmemorykiller is enabled, it would kill processes automatically
// on memory pressure. ArcLowMemoryKillerMonitor listens on lowmemorykiller log
// from kernel space and reports to UMA.
//
// Note: There should be only one ArcLowMemoryKillerMonitor instance during the
// power cycle of a Chrome OS device, otherwise an lowmemorykiller event would
// be reported twice since it reads in the entire kernel log from head.
// On Chrome OS, it should live within a main browser thread.
class ArcLowMemoryKillerMonitor {
 public:
  ArcLowMemoryKillerMonitor();
  ~ArcLowMemoryKillerMonitor();

  void Start();
  void Stop();

 private:
  // Keep a reference to worker_pool_ in case |this| is deleted in
  // shutdown process while this thread returns from a blocking read.
  static void Run(scoped_refptr<base::SequencedWorkerPool> worker_pool);

  scoped_refptr<base::SequencedWorkerPool> worker_pool_;

  DISALLOW_COPY_AND_ASSIGN(ArcLowMemoryKillerMonitor);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_METRICS_ARC_LOW_MEMORY_KILLER_MONITOR_H_
