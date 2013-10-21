// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cc/debug/micro_benchmark.h"
#include "cc/debug/micro_benchmark_controller.h"
#include "cc/layers/layer.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MicroBenchmarkControllerTest : public testing::Test {
 public:
  virtual void SetUp() {
    layer_tree_host_ = FakeLayerTreeHost::Create();
    layer_tree_host_->SetRootLayer(Layer::Create());
    layer_tree_host_->InitializeForTesting(
      scoped_ptr<Proxy>(new FakeProxy));
  }

  scoped_ptr<FakeLayerTreeHost> layer_tree_host_;
};

void Noop(scoped_ptr<base::Value> value) {
}

void IncrementCallCount(int* count, scoped_ptr<base::Value> value) {
  ++(*count);
}

TEST_F(MicroBenchmarkControllerTest, ScheduleFail) {
  bool result = layer_tree_host_->ScheduleMicroBenchmark(
      "non_existant_benchmark", scoped_ptr<base::Value>(), base::Bind(&Noop));
  EXPECT_FALSE(result);
}

TEST_F(MicroBenchmarkControllerTest, CommitScheduled) {
  EXPECT_FALSE(layer_tree_host_->needs_commit());
  bool result = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark", scoped_ptr<base::Value>(), base::Bind(&Noop));
  EXPECT_TRUE(result);
  EXPECT_TRUE(layer_tree_host_->needs_commit());
}

TEST_F(MicroBenchmarkControllerTest, BenchmarkRan) {
  int run_count = 0;
  bool result = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_TRUE(result);

  scoped_ptr<ResourceUpdateQueue> queue(new ResourceUpdateQueue);
  layer_tree_host_->SetOutputSurfaceLostForTesting(false);
  layer_tree_host_->UpdateLayers(queue.get());

  EXPECT_EQ(1, run_count);
}

TEST_F(MicroBenchmarkControllerTest, MultipleBenchmarkRan) {
  int run_count = 0;
  bool result = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_TRUE(result);
  result = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_TRUE(result);

  scoped_ptr<ResourceUpdateQueue> queue(new ResourceUpdateQueue);
  layer_tree_host_->SetOutputSurfaceLostForTesting(false);
  layer_tree_host_->UpdateLayers(queue.get());

  EXPECT_EQ(2, run_count);

  result = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_TRUE(result);
  result = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark",
      scoped_ptr<base::Value>(),
      base::Bind(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_TRUE(result);

  layer_tree_host_->UpdateLayers(queue.get());
  EXPECT_EQ(4, run_count);

  layer_tree_host_->UpdateLayers(queue.get());
  EXPECT_EQ(4, run_count);
}

}  // namespace
}  // namespace cc
