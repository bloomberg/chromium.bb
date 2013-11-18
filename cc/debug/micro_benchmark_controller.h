// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_MICRO_BENCHMARK_CONTROLLER_H_
#define CC_DEBUG_MICRO_BENCHMARK_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "cc/base/scoped_ptr_vector.h"
#include "cc/debug/micro_benchmark.h"

namespace base {
class Value;
class MessageLoopProxy;
}  // namespace base

namespace cc {

class LayerTreeHost;
class LayerTreeHostImpl;
class CC_EXPORT MicroBenchmarkController {
 public:
  explicit MicroBenchmarkController(LayerTreeHost* host);
  ~MicroBenchmarkController();

  void DidUpdateLayers();

  bool ScheduleRun(const std::string& micro_benchmark_name,
                   scoped_ptr<base::Value> value,
                   const MicroBenchmark::DoneCallback& callback);

  void ScheduleImplBenchmarks(LayerTreeHostImpl* host_impl);

 private:
  void CleanUpFinishedBenchmarks();

  LayerTreeHost* host_;
  ScopedPtrVector<MicroBenchmark> benchmarks_;
  scoped_refptr<base::MessageLoopProxy> main_controller_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(MicroBenchmarkController);
};

}  // namespace cc

#endif  // CC_DEBUG_MICRO_BENCHMARK_CONTROLLER_H_
