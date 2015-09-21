// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/page_load_metrics/common/page_load_timing.h"

namespace base {
class Timer;
}  // namespace base

namespace IPC {
class Sender;
}  // namespace IPC

namespace page_load_metrics {

// PageTimingMetricsSender is responsible for sending page load timing metrics
// over IPC. PageTimingMetricsSender may coalesce sent IPCs in order to
// minimize IPC contention.
class PageTimingMetricsSender {
 public:
  PageTimingMetricsSender(IPC::Sender* ipc_sender,
                          int routing_id,
                          scoped_ptr<base::Timer> timer);
  ~PageTimingMetricsSender();

  void Send(const PageLoadTiming& timing);

 protected:
  base::Timer* timer() const { return timer_.get(); }

 private:
  void SendNow();

  IPC::Sender* const ipc_sender_;
  const int routing_id_;
  scoped_ptr<base::Timer> timer_;
  PageLoadTiming last_timing_;

  DISALLOW_COPY_AND_ASSIGN(PageTimingMetricsSender);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_PAGE_TIMING_METRICS_SENDER_H_
