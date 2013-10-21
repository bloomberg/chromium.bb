// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/micro_benchmark_controller.h"

#include <string>

#include "base/callback.h"
#include "base/values.h"
#include "cc/debug/picture_record_benchmark.h"
#include "cc/debug/unittest_only_benchmark.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

namespace {

scoped_ptr<MicroBenchmark> CreateBenchmark(
    const std::string& name,
    scoped_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback) {
  if (name == "picture_record_benchmark") {
    return scoped_ptr<MicroBenchmark>(
        new PictureRecordBenchmark(value.Pass(), callback));
  } else if (name == "unittest_only_benchmark") {
    return scoped_ptr<MicroBenchmark>(
        new UnittestOnlyBenchmark(value.Pass(), callback));
  }
  return scoped_ptr<MicroBenchmark>();
}

class IsDonePredicate {
 public:
  typedef const MicroBenchmark* argument_type;
  typedef bool result_type;

  result_type operator()(argument_type benchmark) const {
    return benchmark->IsDone();
  }
};

}  // namespace

MicroBenchmarkController::MicroBenchmarkController(LayerTreeHost* host)
    : host_(host) {
  DCHECK(host_);
}

MicroBenchmarkController::~MicroBenchmarkController() {}

bool MicroBenchmarkController::ScheduleRun(
    const std::string& micro_benchmark_name,
    scoped_ptr<base::Value> value,
    const MicroBenchmark::DoneCallback& callback) {
  scoped_ptr<MicroBenchmark> benchmark =
      CreateBenchmark(micro_benchmark_name, value.Pass(), callback);
  if (benchmark.get()) {
    benchmarks_.push_back(benchmark.Pass());
    host_->SetNeedsCommit();
    return true;
  }
  return false;
}

void MicroBenchmarkController::DidUpdateLayers() {
  for (ScopedPtrVector<MicroBenchmark>::iterator it = benchmarks_.begin();
       it != benchmarks_.end();
       ++it) {
    DCHECK(!(*it)->IsDone());
    (*it)->DidUpdateLayers(host_);
  }

  CleanUpFinishedBenchmarks();
}

void MicroBenchmarkController::CleanUpFinishedBenchmarks() {
  benchmarks_.erase(
      benchmarks_.partition(std::not1(IsDonePredicate())),
      benchmarks_.end());
}

}  // namespace cc
