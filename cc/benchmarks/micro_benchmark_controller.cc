// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/micro_benchmark_controller.h"

#include <limits>
#include <string>

#include "base/callback.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "cc/benchmarks/invalidation_benchmark.h"
#include "cc/benchmarks/rasterize_and_record_benchmark.h"
#include "cc/benchmarks/unittest_only_benchmark.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_host_impl.h"

namespace cc {

int MicroBenchmarkController::next_id_ = 1;

namespace {

std::unique_ptr<MicroBenchmark> CreateBenchmark(
    const std::string& name,
    std::unique_ptr<base::Value> value,
    MicroBenchmark::DoneCallback callback) {
  if (name == "invalidation_benchmark") {
    return std::make_unique<InvalidationBenchmark>(std::move(value),
                                                   std::move(callback));
  } else if (name == "rasterize_and_record_benchmark") {
    return std::make_unique<RasterizeAndRecordBenchmark>(std::move(value),
                                                         std::move(callback));
  } else if (name == "unittest_only_benchmark") {
    return std::make_unique<UnittestOnlyBenchmark>(std::move(value),
                                                   std::move(callback));
  }
  return nullptr;
}

}  // namespace

MicroBenchmarkController::MicroBenchmarkController(LayerTreeHost* host)
    : host_(host),
      main_controller_task_runner_(base::ThreadTaskRunnerHandle::IsSet()
                                       ? base::ThreadTaskRunnerHandle::Get()
                                       : nullptr) {
  DCHECK(host_);
}

MicroBenchmarkController::~MicroBenchmarkController() = default;

int MicroBenchmarkController::ScheduleRun(
    const std::string& micro_benchmark_name,
    std::unique_ptr<base::Value> value,
    MicroBenchmark::DoneCallback callback) {
  std::unique_ptr<MicroBenchmark> benchmark = CreateBenchmark(
      micro_benchmark_name, std::move(value), std::move(callback));
  if (benchmark.get()) {
    int id = GetNextIdAndIncrement();
    benchmark->set_id(id);
    benchmarks_.push_back(std::move(benchmark));
    host_->SetNeedsCommit();
    return id;
  }
  return 0;
}

int MicroBenchmarkController::GetNextIdAndIncrement() {
  int id = next_id_++;
  // Wrap around to 1 if we overflow (very unlikely).
  if (next_id_ == std::numeric_limits<int>::max())
    next_id_ = 1;
  return id;
}

bool MicroBenchmarkController::SendMessage(int id,
                                           std::unique_ptr<base::Value> value) {
  auto it =
      std::find_if(benchmarks_.begin(), benchmarks_.end(),
                   [id](const std::unique_ptr<MicroBenchmark>& benchmark) {
                     return benchmark->id() == id;
                   });
  if (it == benchmarks_.end())
    return false;
  return (*it)->ProcessMessage(std::move(value));
}

void MicroBenchmarkController::ScheduleImplBenchmarks(
    LayerTreeHostImpl* host_impl) {
  for (const auto& benchmark : benchmarks_) {
    std::unique_ptr<MicroBenchmarkImpl> benchmark_impl;
    if (!benchmark->ProcessedForBenchmarkImpl()) {
      benchmark_impl =
          benchmark->GetBenchmarkImpl(main_controller_task_runner_);
    }

    if (benchmark_impl.get())
      host_impl->ScheduleMicroBenchmark(std::move(benchmark_impl));
  }
}

void MicroBenchmarkController::DidUpdateLayers() {
  for (const auto& benchmark : benchmarks_) {
    if (!benchmark->IsDone())
      benchmark->DidUpdateLayers(host_);
  }

  CleanUpFinishedBenchmarks();
}

void MicroBenchmarkController::CleanUpFinishedBenchmarks() {
  base::EraseIf(benchmarks_,
                [](const std::unique_ptr<MicroBenchmark>& benchmark) {
                  return benchmark->IsDone();
                });
}

}  // namespace cc
